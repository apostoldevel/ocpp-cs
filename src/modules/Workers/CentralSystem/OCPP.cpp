/*++

Library name:

  apostol-core

Module Name:

  OCPP.cpp

Notices:

  Open Charge Point Protocol (OCPP)

  OCPP-S 1.5
  OCPP-J 1.6

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Header.hpp"
#include "OCPP.hpp"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace OCPP {

        CString IntToString(int Value) {
            CString S;
            S << Value;
            return S;
        }
        //--------------------------------------------------------------------------------------------------------------

        CString GetISOTime(long int Delta = 0) {
            CString S;
            TCHAR buffer[80] = {0};

            struct timeval now = {};
            struct tm *timeinfo = nullptr;

            gettimeofday(&now, nullptr);
            if (Delta > 0) now.tv_sec += Delta;
            timeinfo = gmtime(&now.tv_sec);

            strftime(buffer, sizeof buffer, "%FT%T", timeinfo);

            S.Format("%s.%03ldZ", buffer, now.tv_usec / 1000);

            return S;
        }
        //--------------------------------------------------------------------------------------------------------------

        void StopTransactionRequest::XMLToMeterValue(const CString &xmlString, CMeterValue &MeterValue) {
            CStringPairs Values;

            xml_document<> xmlDocument;
            xmlDocument.parse<0>((char *) xmlString.c_str());

            xml_node<> *timeStamp = xmlDocument.first_node("timestamp");
            MeterValue.timestamp = StrToDateTimeDef(timeStamp->first_node("timestamp")->value(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

            xml_node<> *sampledValue = xmlDocument.first_node("sampledValue");
            for (xml_node<> *value = sampledValue->first_node(); value; value = value->next_sibling()) {
                Values.AddPair(value->name(), value->value());
            }

            MeterValue.sampledValue << Values;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::Request(const CString &xmlString, CSOAPMessage &Message) {
            xml_document<> xmlDocument;
            xmlDocument.parse<0>((char *) xmlString.c_str());

            xml_node<> *root = xmlDocument.first_node("s:Envelope");

            xml_node<> *headers = root->first_node("s:Header");
            for (xml_node<> *header = headers->first_node(); header; header = header->next_sibling()) {
                xml_node<> *address = header->first_node("a:Address");
                if (address)
                    Message.Headers.AddPair(header->name(), address->value());
                else
                    Message.Headers.AddPair(header->name(), header->value());
            }

            xml_node<> *body = root->first_node("s:Body");

            xml_node<> *notification = body->first_node();
            Message.Notification = notification->name();

            xml_node<> *values = body->first_node(notification->name());
            for (xml_node<> *value = values->first_node(); value; value = value->next_sibling()) {
                Message.Values.AddPair(value->name(), value->value());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::Response(const CSOAPMessage &Message, CString &xmlString) {

            const CStringPairs &Headers = Message.Headers;
            const CStringPairs &Values = Message.Values;

            xmlString =  R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED;
            xmlString << R"(<s:Envelope xmlns:a="http://www.w3.org/2005/08/addressing")" LINEFEED;
            xmlString << R"(xmlns:s="http://www.w3.org/2003/05/soap-envelope")" LINEFEED;
            xmlString << R"(xmlns="urn://Ocpp/Cs/2012/06/">)" LINEFEED;

            xmlString << R"(<s:Header>)" LINEFEED;

            for (int i = 0; i < Headers.Count(); i++ ) {
                const CString& Name = Headers[i].Name();
                const CString& Value = Headers[i].Value();

                if (Name == "a:From" || Name == "a:ReplyTo" || Name == "a:FaultTo")
                    xmlString.Format(R"(<%s><a:Address>%s</a:Address></%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
                else
                    xmlString.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            xmlString << R"(</s:Header>)" LINEFEED;
            xmlString << R"(<s:Body>)" LINEFEED;

            xmlString.Format(R"(<%s>)" LINEFEED, Message.Notification.c_str());

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name();
                const CString& Value = Values[i].Value();

                xmlString.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            xmlString.Format(R"(</%s>)" LINEFEED, Message.Notification.c_str());

            xmlString << R"(</s:Body>)" LINEFEED;
            xmlString << R"(</s:Envelope>)";
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::PrepareResponse(const CSOAPMessage &Request, CSOAPMessage &Response) {

            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Values = Request.Values;

            const CString &Identity = Headers.Values("chargeBoxIdentity");
            const CString &MessageID = Headers.Values("a:MessageID");
            const CString &From = Headers.Values("a:From");
            const CString &To = Headers.Values("a:To");
            const CString &ReplyTo = Headers.Values("a:ReplyTo");
            const CString &FaultTo = Headers.Values("a:FaultTo");
            const CString &Action = Headers.Values("a:Action");

            Response.Headers.AddPair("chargeBoxIdentity", Identity);
            Response.Headers.AddPair("a:MessageID", MessageID);
            Response.Headers.AddPair("a:From", To); // <- It is right, swap!!!
            Response.Headers.AddPair("a:To", From); // <- It is right, swap!!!
            Response.Headers.AddPair("a:ReplyTo", ReplyTo);
            Response.Headers.AddPair("a:FaultTo", FaultTo);
            Response.Headers.AddPair("a:Action", Action + "Response");
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CJSONProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        bool CJSONProtocol::Request(const CString &String, CJSONMessage &Message) {
            bool Result = false;

            if (String.Size() > 0) {

                CString Item;
                size_t Pos = 0;

                bool Quotes = false;
                int Brackets = 0;

                CJSONParserState State = psBegin;

                TCHAR ch = String.at(Pos++);

                while (ch != 0) {

                    switch (State) {
                        case psBegin:

                            if (ch == ' ')
                                break;

                            if (ch != '[')
                                throw Delphi::Exception::Exception("This is not OCPP-J protocol.");

                            State = psMessageTyeId;
                            break;

                        case psMessageTyeId:

                            if (ch == ' ')
                                break;

                            if (ch == ',') {
                                State = psUniqueId;
                                break;
                            }

                            switch (ch) {
                                case '2':
                                    Message.MessageTypeId = mtCall;
                                    break;
                                case '3':
                                    Message.MessageTypeId = mtCallResult;
                                    break;
                                case '4':
                                    Message.MessageTypeId = mtCallError;
                                    break;
                                default:
                                    throw Delphi::Exception::Exception("Invalid \"MessageTypeId\" value.");
                            }

                            break;

                        case psUniqueId:

                            if (!Quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                Quotes = !Quotes;
                                break;
                            }

                            if (!Quotes && ch == ',') {
                                switch (Message.MessageTypeId) {
                                    case mtCall:
                                        State = psAction;
                                        break;
                                    case mtCallResult:
                                        State = psPayloadBegin;
                                        break;
                                    case mtCallError:
                                        State = psErrorCode;
                                        break;
                                }

                                break;
                            }

                            Message.UniqueId.Append(ch);
                            break;

                        case psAction:

                            if (!Quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                Quotes = !Quotes;
                                break;
                            }

                            if (!Quotes && ch == ',') {
                                State = psPayloadBegin;
                                break;
                            }

                            Message.Action.Append(ch);
                            break;

                        case psErrorCode:

                            if (!Quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                Quotes = !Quotes;
                                break;
                            }

                            if (!Quotes && ch == ',') {
                                State = psErrorDescription;
                                break;
                            }

                            Message.ErrorCode.Append(ch);
                            break;

                        case psErrorDescription:

                            if (!Quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                Quotes = !Quotes;
                                break;
                            }

                            if (!Quotes && ch == ',') {
                                State = psPayloadBegin;
                                break;
                            }

                            Message.ErrorDescription.Append(ch);
                            break;

                        case psPayloadBegin:

                            if (ch == '{') {
                                Brackets++;
                                Item.Append(ch);
                                State = psPayloadObject;
                            } else if (ch == '[') {
                                Brackets++;
                                Item.Append(ch);
                                State = psPayloadArray;
                            }

                            break;

                        case psPayloadObject:

                            if (ch == '{') {
                                Brackets++;
                            } else if (ch == '}') {
                                Brackets--;
                            }

                            Item.Append(ch);

                            if (Brackets == 0) {
                                Message.Payload << Item;
                                State = psEnd;
                            }

                            break;

                        case psPayloadArray:

                            if (ch == '[') {
                                Brackets++;
                            } else if (ch == ']') {
                                Brackets--;
                            }

                            Item.Append(ch);

                            if (Brackets == 0) {
                                Message.Payload << Item;
                                State = psEnd;
                            }

                            break;

                        case psEnd:

                            if (ch == ']')
                                Result = true;

                            break;
                    }

                    ch = String.at(Pos++);
                }
            }

            return Result;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CJSONProtocol::Response(const CJSONMessage &Message, CString &String) {

            const auto& Payload = Message.Payload.ToString();
            const size_t Size = Message.Size() + Payload.Size();

            String.MaxFormatSize(64 + Size);

            switch (Message.MessageTypeId) {
                case mtCall:
                    String.Format(R"([2,"%s","%s",%s])",
                                  Message.UniqueId.c_str(),
                                  Message.Action.c_str(),
                                  Payload.IsEmpty() ? "{}" : Payload.c_str()
                    );
                    break;

                case mtCallResult:
                    String.Format(R"([3,"%s",%s])",
                                  Message.UniqueId.c_str(),
                                  Payload.IsEmpty() ? "{}" : Payload.c_str()
                    );
                    break;

                case mtCallError:
                    String.Format(R"([4,"%s","%s","%s",%s])",
                                  Message.UniqueId.c_str(),
                                  Message.ErrorCode.c_str(),
                                  Message.ErrorDescription.c_str(),
                                  Payload.IsEmpty() ? "{}" : Payload.c_str()
                    );
                    break;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CJSONProtocol::PrepareResponse(const CJSONMessage &Request, CJSONMessage &Response) {
            Response.MessageTypeId = mtCallResult;
            Response.UniqueId = Request.UniqueId;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CJSONProtocol::Call(const CString &UniqueId, const CString &Action, const CJSON &Payload, CString &Result) {
            CJSONMessage Message;
            Message.MessageTypeId = mtCall;
            Message.UniqueId = UniqueId;
            Message.Action = Action;
            Message.Payload = Payload;
            Response(Message, Result);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CJSONProtocol::CallResult(const CString &UniqueId, const CJSON &Payload, CString &Result) {
            CJSONMessage Message;
            Message.MessageTypeId = mtCallResult;
            Message.UniqueId = UniqueId;
            Message.Payload = Payload;
            Response(Message, Result);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CJSONProtocol::CallError(const CString &UniqueId, const CString &ErrorCode, const CString &ErrorDescription,
                                 const CJSON &Payload, CString &Result) {

            CJSONMessage Message;
            Message.MessageTypeId = mtCallError;
            Message.UniqueId = UniqueId;
            Message.ErrorCode = ErrorCode;
            Message.ErrorDescription = ErrorDescription;
            Message.Payload = Payload;
            Response(Message, Result);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CChargePointStatus COCPPMessage::StringToChargePointStatus(const CString &Value) {
            if (Value == "Available")
                return cpsAvailable;
            if (Value == "Occupied")
                return cpsOccupied;
            if (Value == "Faulted")
                return cpsFaulted;
            if (Value == "Unavailable")
                return cpsUnavailable;
            if (Value == "Reserved")
                return cpsReserved;
            return cpsUnknown;
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ChargePointStatusToString(CChargePointStatus Value) {
            switch (Value) {
                case cpsUnknown:
                    return "Unknown";
                case cpsAvailable:
                    return "Available";
                case cpsOccupied:
                    return "Occupied";
                case cpsFaulted:
                    return "Faulted";
                case cpsUnavailable:
                    return "Unavailable";
                case cpsReserved:
                    return "Reserved";
                default:
                    throw ExceptionFrm("Invalid \"ChargePointStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargePointErrorCode COCPPMessage::StringToChargePointErrorCode(const CString &Value) {
            if (Value == "ConnectorLockFailure")
                return cpeConnectorLockFailure;
            if (Value == "HighTemperature")
                return cpeHighTemperature;
            if (Value == "Mode3Error")
                return cpeMode3Error;
            if (Value == "NoError")
                return cpeNoError;
            if (Value == "PowerMeterFailure")
                return cpePowerMeterFailure;
            if (Value == "PowerSwitchFailure")
                return cpePowerSwitchFailure;
            if (Value == "ReaderFailure")
                return cpeReaderFailure;
            if (Value == "ResetFailure")
                return cpeResetFailure;
            if (Value == "GroundFailure")
                return cpeGroundFailure;
            if (Value == "OverCurrentFailure")
                return cpeOverCurrentFailure;
            if (Value == "UnderVoltage")
                return cpeUnderVoltage;
            if (Value == "WeakSignal")
                return cpeWeakSignal;
            if (Value == "OtherError")
                return cpeOtherError;
            return cpeUnknown;
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ChargePointErrorCodeToString(CChargePointErrorCode Value) {
            switch (Value) {
                case cpeUnknown:
                    return "Unknown";
                case cpeConnectorLockFailure:
                    return "ConnectorLockFailure";
                case cpeHighTemperature:
                    return "HighTemperature";
                case cpeMode3Error:
                    return "Mode3Error";
                case cpeNoError:
                    return "NoError";
                case cpePowerMeterFailure:
                    return "PowerMeterFailure";
                case cpePowerSwitchFailure:
                    return "PowerSwitchFailure";
                case cpeReaderFailure:
                    return "ReaderFailure";
                case cpeResetFailure:
                    return "ResetFailure";
                case cpeGroundFailure:
                    return "GroundFailure";
                case cpeOverCurrentFailure:
                    return "OverCurrentFailure";
                case cpeUnderVoltage:
                    return "UnderVoltage";
                case cpeWeakSignal:
                    return "WeakSignal";
                case cpeOtherError:
                    return "OtherError";
                default:
                    throw ExceptionFrm("Invalid \"ChargePointErrorCode\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool COCPPMessage::Parse(CChargingPoint *APoint, const CSOAPMessage &Request, CSOAPMessage &Response) {

            const auto& Action = Request.Notification.Lower();

            CSOAPProtocol::PrepareResponse(Request, Response);

            if (Action == "authorizerequest") {
                APoint->Authorize(Request, Response);
                return true;
            } else if (Action == "starttransactionrequest") {
                APoint->StartTransaction(Request, Response);
                return true;
            } else if (Action == "stoptransactionrequest") {
                APoint->StopTransaction(Request, Response);
                return true;
            } else if (Action == "bootnotificationrequest") {
                APoint->BootNotification(Request, Response);
                return true;
            } else if (Action == "statusnotificationrequest") {
                APoint->StatusNotification(Request, Response);
                return true;
            } else if (Action == "datatransferrequest") {
                APoint->DataTransfer(Request, Response);
                return true;
            } else if (Action == "heartbeatrequest") {
                APoint->Heartbeat(Request, Response);
                return true;
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool COCPPMessage::Parse(CChargingPoint *APoint, const CJSONMessage &Request, CJSONMessage &Response) {
            if (Request.MessageTypeId == mtCall) {
                const auto &Action = Request.Action.Lower();

                CJSONProtocol::PrepareResponse(Request, Response);

                if (Action == "authorize") {
                    APoint->Authorize(Request, Response);
                } else if (Action == "starttransaction") {
                    APoint->StartTransaction(Request, Response);
                } else if (Action == "stoptransaction") {
                    APoint->StopTransaction(Request, Response);
                } else if (Action == "bootnotification") {
                    APoint->BootNotification(Request, Response);
                } else if (Action == "statusnotification") {
                    APoint->StatusNotification(Request, Response);
                } else if (Action == "datatransfer") {
                    APoint->DataTransfer(Request, Response);
                } else if (Action == "heartbeat") {
                    APoint->Heartbeat(Request, Response);
                } else {
                    return false;
                }

                return true;
            }

            return false;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CMessageHandler -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CMessageHandler::CMessageHandler(CMessageManager *AManager, COnMessageHandlerEvent &&Handler) :
                CCollectionItem(AManager), m_Handler(Handler) {
            m_UniqueId = GetUID(APOSTOL_MODULE_UID_LENGTH);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CMessageHandler::Handler(CHTTPServerConnection *AConnection) {
            if (m_Handler != nullptr) {
                m_Handler(this, AConnection);
            }
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CMessageManager -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CMessageHandler *CMessageManager::Get(int Index) {
            return dynamic_cast<CMessageHandler *> (inherited::GetItem(Index));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CMessageManager::Set(int Index, CMessageHandler *Value) {
            inherited::SetItem(Index, Value);
        }
        //--------------------------------------------------------------------------------------------------------------

        CMessageHandler *CMessageManager::Add(COnMessageHandlerEvent &&Handler, const CString &Action,
                const CJSON &Payload) {
            auto LHandler = new CMessageHandler(this, static_cast<COnMessageHandlerEvent &&>(Handler));
            auto LConnection = m_Point->Connection();

            LHandler->Action() = Action;

            CString LResult;

            CJSONProtocol::Call(LHandler->UniqueId(), Action, Payload, LResult);

            auto LWSReply = LConnection->WSReply();

            LWSReply->Clear();
            LWSReply->SetPayload(LResult);

            LConnection->SendWebSocket(true);

            return LHandler;
        }
        //--------------------------------------------------------------------------------------------------------------

        CMessageHandler *CMessageManager::FindMessageById(const CString &Value) {
            CMessageHandler *Handler = nullptr;

            for (int I = 0; I < Count(); ++I) {
                Handler = Get(I);
                if (Handler->UniqueId() == Value)
                    return Handler;
            }

            return nullptr;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint::CChargingPoint(CHTTPServerConnection *AConnection, CChargingPointManager *AManager) : CCollectionItem(AManager) {
            m_pConnection = AConnection;
            m_TransactionId = 0;
            m_Messages = new CMessageManager(this);
            AddToConnection(AConnection);
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint::~CChargingPoint() {
            DeleteFromConnection(m_pConnection);
            m_pConnection = nullptr;
            FreeAndNil(m_Messages);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::AddToConnection(CHTTPServerConnection *AConnection) {
            if (Assigned(AConnection)) {
                int Index = AConnection->Data().IndexOfName("point");
                if (Index == -1) {
                    AConnection->Data().AddObject("point", this);
                } else {
                    delete AConnection->Data().Objects(Index);
                    AConnection->Data().Objects(Index, this);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::DeleteFromConnection(CHTTPServerConnection *AConnection) {
            if (Assigned(AConnection)) {
                int Index = AConnection->Data().IndexOfObject(this);
                if (Index != -1)
                    AConnection->Data().Delete(Index);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SwitchConnection(CHTTPServerConnection *AConnection) {
            if (m_pConnection != AConnection) {
                DeleteFromConnection(m_pConnection);
                m_pConnection->Disconnect();
                m_pConnection = AConnection;
                AddToConnection(m_pConnection);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint *CChargingPoint::FindOfConnection(CHTTPServerConnection *AConnection) {
            int Index = AConnection->Data().IndexOfName("point");
            if (Index == -1)
                throw Delphi::Exception::ExceptionFrm("Not found charging point in connection");

            auto Object = AConnection->Data().Objects(Index);
            if (Object == nullptr)
                throw Delphi::Exception::ExceptionFrm("Object in connection data is null");

            auto Point = dynamic_cast<CChargingPoint *> (Object);
            if (Point == nullptr)
                throw Delphi::Exception::Exception("Charging point is null");

            return Point;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Authorize(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_AuthorizeRequest << Body;

            Response.Notification = "authorizeResponse";

            CString IdTagInfo(LINEFEED);
            CStringPairs Values;

            Values.AddPair("status", "Accepted");
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name();
                const CString& Value = Values[i].Value();

                IdTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Authorize(const CJSONMessage &Request, CJSONMessage &Response) {

            m_AuthorizeRequest << Request.Payload;

            CJSONValue IdTagInfo(jvtObject);

            IdTagInfo.Object().AddPair("status", "Accepted");
            IdTagInfo.Object().AddPair("expiryDate", GetISOTime(5 * 60));

            auto& Values = Response.Payload.Object();
            Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StartTransaction(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StartTransactionRequest << Body;

            Response.Notification = "startTransactionResponse";

            CString IdTagInfo(LINEFEED);
            CStringPairs Values;

            Response.Values.AddPair("transactionId", IntToString(++m_TransactionId));

            Values.AddPair("status", "Accepted");
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name();
                const CString& Value = Values[i].Value();

                IdTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StartTransaction(const CJSONMessage &Request, CJSONMessage &Response) {

            CJSONValue IdTagInfo(jvtObject);

            IdTagInfo.Object().AddPair("status", "Accepted");
            IdTagInfo.Object().AddPair("expiryDate", GetISOTime(5 * 60));

            auto& Values = Response.Payload.Object();

            Values.AddPair("idTagInfo", IdTagInfo);
            Values.AddPair("transactionId", IntToString(++m_TransactionId));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StopTransaction(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StopTransactionRequest << Body;

            Response.Notification = "stopTransactionResponse";

            CString IdTagInfo(LINEFEED);
            CStringPairs Values;

            Values.AddPair("status", "Accepted");
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name();
                const CString& Value = Values[i].Value();

                IdTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StopTransaction(const CJSONMessage &Request, CJSONMessage &Response) {
            CJSONValue IdTagInfo(jvtObject);

            IdTagInfo.Object().AddPair("status", "Accepted");
            IdTagInfo.Object().AddPair("expiryDate", GetISOTime(5 * 60));

            auto& Values = Response.Payload.Object();

            Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::BootNotification(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_BootNotificationRequest << Body;

            Response.Notification = "bootNotificationResponse";

            auto& Values = Response.Values;

            Values.AddPair("status", "Accepted");
            Values.AddPair("currentTime", GetISOTime());
            Values.AddPair("heartbeatInterval", "60");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::BootNotification(const CJSONMessage &Request, CJSONMessage &Response) {

            m_BootNotificationRequest << Request.Payload;

            Response.MessageTypeId = mtCallResult;

            auto& Values = Response.Payload.Object();

            Values.AddPair("status", "Accepted");
            Values.AddPair("currentTime", GetISOTime());
            Values.AddPair("interval", 60);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StatusNotification(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StatusNotificationRequest << Body;

            Response.Notification = "statusNotificationResponse";
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StatusNotification(const CJSONMessage &Request, CJSONMessage &Response) {
            m_StatusNotificationRequest << Request.Payload;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::DataTransfer(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_DataTransferRequest << Body;

            Response.Notification = "dataTransferResponse";

            auto& Values = Response.Values;
            Values.AddPair("status", "Accepted");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::DataTransfer(const CJSONMessage &Request, CJSONMessage &Response) {
            m_DataTransferRequest << Request.Payload;

            auto& Values = Response.Payload.Object();
            Values.AddPair("status", "Accepted");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Heartbeat(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            Response.Notification = "heartbeatResponse";

            auto& Values = Response.Values;
            Values.AddPair("currentTime", GetISOTime());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Heartbeat(const CJSONMessage &Request, CJSONMessage &Response) {
            auto& Values = Response.Payload.Object();
            Values.AddPair("currentTime", GetISOTime());
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CChargingPoint::ParseSOAP(const CString &Request, CString &Response) {
            CSOAPMessage LRequest;
            CSOAPMessage LResponse;

            CSOAPProtocol::Request(Request, LRequest);
            if (COCPPMessage::Parse(this, LRequest, LResponse)) {
                CSOAPProtocol::Response(LResponse, Response);
                return true;
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CChargingPoint::ParseJSON(const CString &Request, CString &Response) {
            CJSONMessage LRequest;
            CJSONMessage LResponse;

            CJSONProtocol::Request(Request, LRequest);
            if (LRequest.MessageTypeId == mtCall) {
                if (COCPPMessage::Parse(this, LRequest, LResponse)) {
                    CJSONProtocol::Response(LResponse, Response);
                    return true;
                }
            } else {
                auto LHandler = m_Messages->FindMessageById(LRequest.UniqueId);
                if (Assigned(LHandler)) {
                    LHandler->Handler(m_pConnection);
                    return true;
                }
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CChargingPoint::Parse(CProtocolType Protocol, const CString &Request, CString &Response) {
            switch (Protocol) {
                case ptSOAP:
                    return ParseSOAP(Request, Response);
                case ptJSON:
                    return ParseJSON(Request, Response);
            }
            return false;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPointManager -------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint *CChargingPointManager::Get(int Index) {
            return dynamic_cast<CChargingPoint *> (inherited::GetItem(Index));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPointManager::Set(int Index, CChargingPoint *Value) {
            inherited::SetItem(Index, Value);
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint *CChargingPointManager::Add(CHTTPServerConnection *AConnection) {
            return new CChargingPoint(AConnection, this);
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint *CChargingPointManager::FindPointByIdentity(const CString &Value) {
            CChargingPoint *Point = nullptr;

            for (int I = 0; I < Count(); ++I) {
                Point = Get(I);
                if (Point->Identity() == Value)
                    return Point;
            }

            return nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint *CChargingPointManager::FindPointByConnection(CHTTPServerConnection *Value) {
            CChargingPoint *Point = nullptr;

            for (int I = 0; I < Count(); ++I) {
                Point = Get(I);
                if (Point->Connection() == Value)
                    return Point;
            }

            return nullptr;
        }

    }
}
}
