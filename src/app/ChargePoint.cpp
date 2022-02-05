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
#include "ChargePoint.hpp"
//----------------------------------------------------------------------------------------------------------------------

#define CP_CONNECTION_DATA_NAME "ChargingPoint"

#define CP_INVALID_CONNECTION_ID "Invalid connectorId: %d"
#define CP_INVALID_TRANSACTION_ID "Invalid transactionId: %d"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace ChargePoint {

        int nTransactionId = 0;

        CString to_string(unsigned long Value) {
            TCHAR szString[_INT_T_LEN + 1] = {0};
            sprintf(szString, "%lu", Value);
            return {szString};
        }
        //--------------------------------------------------------------------------------------------------------------

        CString GenUniqueId() {
            return GetUID(32).Lower();
        }
        //--------------------------------------------------------------------------------------------------------------

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

        void XMLToJSON(xml_node<> *node, CJSON &Json) {
            if (node == nullptr)
                return;

            if (node->type() == node_element) {
                CJSONValue Value;

                XMLToJSON(node->first_node(), Value);

                if (node->value_size() != 0) {
                    Json.Object().AddPair(node->name(), node->value());
                } else {
                    Json.Object().AddPair(node->name(), Value);
                }

                XMLToJSON(node->next_sibling(), Json);
            }
        };
        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::JSONToSOAP(const CString &Identity, const CString &Action, const CString &MessageId, const CString &From, const CString &To, const CJSON &Json, CString &xmlString) {

            const auto& caObject = Json.Object();

            xmlString.Clear();

            xmlString =  R"(<?xml version="1.0" encoding="utf-8"?>)" LINEFEED;
            xmlString << R"(<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:wsa5="http://www.w3.org/2005/08/addressing" xmlns:cp="urn://Ocpp/Cp/2015/10/" xmlns:cs="urn://Ocpp/Cs/2015/10/">)" LINEFEED;
            xmlString << CString().Format(R"(<SOAP-ENV:Header><cs:chargeBoxIdentity SOAP-ENV:mustUnderstand="true">%s</cs:chargeBoxIdentity><wsa5:Action SOAP-ENV:mustUnderstand="true">%s</wsa5:Action><wsa5:MessageID>urn:uuid:%s</wsa5:MessageID><wsa5:From><wsa5:Address>%s</wsa5:Address></wsa5:From><wsa5:To SOAP-ENV:mustUnderstand="true">%s</wsa5:To></SOAP-ENV:Header>)" LINEFEED, Identity.c_str(), Action.c_str(), MessageId.c_str(), From.c_str(), To.c_str());
            xmlString << R"(  <SOAP-ENV:Body>)" LINEFEED;

            for (int i = 0; i < caObject.Count(); i++) {
                const auto& caMember = caObject.Members(i);
                const auto& caString = caMember.String();
                const auto& caValue = caMember.Value().AsString();
                xmlString << CString().Format(R"(    <%s>%s</%s>)" LINEFEED, caString.c_str(), caValue.c_str(), caString.c_str());
            }

            xmlString << R"(  </SOAP-ENV:Body>)" LINEFEED;
            xmlString << R"(</SOAP-ENV:Envelope>)" LINEFEED;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::SOAPToJSON(const CString &xmlString, CJSON &Json) {
            xml_document<> xmlDocument;
            xmlDocument.parse<parse_default>((char *) xmlString.c_str());

            xml_node<> *Envelope = xmlDocument.first_node("SOAP-ENV:Envelope");
            xml_node<> *Body = Envelope->first_node("SOAP-ENV:Body");
            xml_node<> *Response = Body->first_node();

            XMLToJSON(Response, Json);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::Request(const CString &xmlString, CSOAPMessage &Message) {
            xml_document<> xmlDocument;
            xmlDocument.parse<0>((char *) xmlString.c_str());

            xml_node<> *root = xmlDocument.first_node("s:Envelope");

            if (root == nullptr)
                throw Delphi::Exception::ExceptionFrm("Invalid SOAP Header.");

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

            const auto &Headers = Message.Headers;
            const auto &Values = Message.Values;

            xmlString =  R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED;
            xmlString << R"(<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope")" LINEFEED;
            xmlString << R"(xmlns:a="http://www.w3.org/2005/08/addressing")" LINEFEED;
            xmlString << R"(xmlns="urn://Ocpp/Cs/2015/10/">)" LINEFEED;

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

            const auto &Headers = Request.Headers;
            const auto &Values = Request.Values;

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

        void CJSONProtocol::ExceptionToJson(int ErrorCode, const std::exception &e, CJSON &Json) {
            Json.Clear();
            Json << CString().Format(R"({"error": {"code": %u, "message": "%s"}})", ErrorCode, Delphi::Json::EncodeJsonString(e.what()).c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        size_t CJSONProtocol::Request(const CString &String, CJSONMessage &Message) {
            if (String.Position() < String.Size()) {
                CString Item;

                bool quotes = false;
                int brackets = 0;

                CJSONParserState State = psBegin;

                TCHAR ch;

                for (size_t i = String.Position(); i < String.Size(); i++) {

                    ch = String.at(i);

                    switch (State) {
                        case psBegin:

                            if (ch == 13 || ch == 10 || ch == ' ')
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

                            if (!quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                quotes = !quotes;
                                break;
                            }

                            if (!quotes && ch == ',') {
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

                            if (!quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                quotes = !quotes;
                                break;
                            }

                            if (!quotes && ch == ',') {
                                State = psPayloadBegin;
                                break;
                            }

                            Message.Action.Append(ch);
                            break;

                        case psErrorCode:

                            if (!quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                quotes = !quotes;
                                break;
                            }

                            if (!quotes && ch == ',') {
                                State = psErrorDescription;
                                break;
                            }

                            Message.ErrorCode.Append(ch);
                            break;

                        case psErrorDescription:

                            if (!quotes && ch == ' ')
                                break;

                            if (ch == '"') {
                                quotes = !quotes;
                                break;
                            }

                            if (!quotes && ch == ',') {
                                State = psPayloadBegin;
                                break;
                            }

                            Message.ErrorDescription.Append(ch);
                            break;

                        case psPayloadBegin:

                            if (ch == '{') {
                                brackets++;
                                Item.Append(ch);
                                State = psPayloadObject;
                            } else if (ch == '[') {
                                brackets++;
                                Item.Append(ch);
                                State = psPayloadArray;
                            }

                            break;

                        case psPayloadObject:

                            if (ch == '{') {
                                brackets++;
                            } else if (ch == '}') {
                                brackets--;
                            }

                            Item.Append(ch);

                            if (brackets == 0) {
                                try {
                                    Message.Payload << Item;
                                } catch (std::exception &e) {
                                    ExceptionToJson(500, e, Message.Payload);
                                }
                                State = psEnd;
                            }

                            break;

                        case psPayloadArray:

                            if (ch == '[') {
                                brackets++;
                            } else if (ch == ']') {
                                brackets--;
                            }

                            Item.Append(ch);

                            if (brackets == 0) {
                                try {
                                    Message.Payload << Item;
                                } catch (std::exception &e) {
                                    ExceptionToJson(500, e, Message.Payload);
                                }
                                State = psEnd;
                            }

                            break;

                        case psEnd:

                            if (ch == ']')
                                return i + 1;

                            break;
                    }
                }
            }

            return 0;
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
            Response.Action = Request.Action;
        }
        //--------------------------------------------------------------------------------------------------------------

        CJSONMessage CJSONProtocol::Call(const CString &UniqueId, const CString &Action, const CJSON &Payload) {
            CJSONMessage Message;

            Message.MessageTypeId = mtCall;
            Message.UniqueId = UniqueId;
            Message.Action = Action;
            Message.Payload = Payload;

            return Message;
        }
        //--------------------------------------------------------------------------------------------------------------

        CJSONMessage CJSONProtocol::CallResult(const CString &UniqueId, const CJSON &Payload) {
            CJSONMessage Message;

            Message.MessageTypeId = mtCallResult;
            Message.UniqueId = UniqueId;
            Message.Payload = Payload;

            return Message;
        }
        //--------------------------------------------------------------------------------------------------------------

        CJSONMessage CJSONProtocol::CallError(const CString &UniqueId, const CString &ErrorCode,
                const CString &ErrorDescription, const CJSON &Payload) {

            CJSONMessage Message;

            Message.MessageTypeId = mtCallError;
            Message.UniqueId = UniqueId;
            Message.ErrorCode = ErrorCode;
            Message.ErrorDescription = ErrorDescription;
            Message.Payload = Payload;

            return Message;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CMessageType COCPPMessage::StringToMessageTypeId(const CString &Value) {
            if (Value == "CallResult")
                return mtCallResult;
            if (Value == "CallError")
                return mtCallError;
            return mtCall;
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::MessageTypeIdToString(CMessageType Value) {
            switch (Value) {
                case mtCall:
                    return "Call";
                case mtCallResult:
                    return "CallResult";
                case mtCallError:
                    return "CallError";
                default:
                    throw ExceptionFrm("Invalid \"MessageType\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CResetType COCPPMessage::StringToResetType(const CString &Value) {
            if (Value == "Soft")
                return rtSoft;
            if (Value == "Hard")
                return rtHard;
            throw ExceptionFrm("Invalid \"ResetType\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ResetTypeToString(CResetType Value) {
            switch (Value) {
                case rtSoft:
                    return "Soft";
                case rtHard:
                    return "Hard";
                default:
                    throw ExceptionFrm("Invalid \"ResetType\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CMessageTrigger COCPPMessage::StringToMessageTrigger(const CString &Value) {
            if (Value == "BootNotification")
                return mtBootNotification;
            if (Value == "DiagnosticsStatusNotification")
                return mtDiagnosticsStatusNotification;
            if (Value == "FirmwareStatusNotification")
                return mtFirmwareStatusNotification;
            if (Value == "Heartbeat")
                return mtHeartbeat;
            if (Value == "MeterValues")
                return mtMeterValues;
            if (Value == "StatusNotification")
                return mtStatusNotification;
            throw ExceptionFrm("Invalid \"MessageTrigger\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::MessageTriggerToString(CMessageTrigger Value) {
            switch (Value) {
                case mtBootNotification:
                    return "BootNotification";
                case mtDiagnosticsStatusNotification:
                    return "DiagnosticsStatusNotification";
                case mtFirmwareStatusNotification:
                    return "FirmwareStatusNotification";
                case mtHeartbeat:
                    return "Heartbeat";
                case mtMeterValues:
                    return "MeterValues";
                case mtStatusNotification:
                    return "StatusNotification";
                default:
                    throw ExceptionFrm("Invalid \"MessageTrigger\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CAuthorizationStatus COCPPMessage::StringToAuthorizationStatus(const CString &Value) {
            if (Value == "Accepted")
                return asAccepted;
            if (Value == "Blocked")
                return asBlocked;
            if (Value == "Expired")
                return asExpired;
            if (Value == "Invalid")
                return asInvalid;
            if (Value == "ConcurrentTx")
                return asConcurrentTx;
            throw ExceptionFrm("Invalid \"AuthorizationStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::AuthorizationStatusToString(CAuthorizationStatus Value) {
            switch (Value) {
                case asAccepted:
                    return "Accepted";
                case asBlocked:
                    return "Blocked";
                case asExpired:
                    return "Expired";
                case asInvalid:
                    return "Invalid";
                case asConcurrentTx:
                    return "ConcurrentTx";
                default:
                    throw ExceptionFrm("Invalid \"AuthorizationStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CRegistrationStatus COCPPMessage::StringToRegistrationStatus(const CString &Value) {
            if (Value == "Accepted")
                return rsAccepted;
            if (Value == "Pending")
                return rsPending;
            if (Value == "Rejected")
                return rsRejected;
            throw ExceptionFrm("Invalid \"RegistrationStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::RegistrationStatusToString(CRegistrationStatus Value) {
            switch (Value) {
                case rsAccepted:
                    return "Accepted";
                case rsPending:
                    return "Pending";
                case rsRejected:
                    return "Rejected";
                default:
                    throw ExceptionFrm("Invalid \"RegistrationStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CRemoteStartStopStatus COCPPMessage::StringToRemoteStartStopStatus(const CString &Value) {
            if (Value == "Accepted")
                return rssAccepted;
            if (Value == "Rejected")
                return rssRejected;
            throw ExceptionFrm("Invalid \"RemoteStartStopStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::RemoteStartStopStatusToString(CRemoteStartStopStatus Value) {
            switch (Value) {
                case rssAccepted:
                    return "Accepted";
                case rssRejected:
                    return "Rejected";
                default:
                    throw ExceptionFrm("Invalid \"RemoteStartStopStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CReservationStatus COCPPMessage::StringToReservationStatus(const CString &Value) {
            if (Value == "Accepted")
                return rvsAccepted;
            if (Value == "Faulted")
                return rvsFaulted;
            if (Value == "Occupied")
                return rvsOccupied;
            if (Value == "Rejected")
                return rvsRejected;
            if (Value == "Unavailable")
                return rvsUnavailable;
            throw ExceptionFrm("Invalid \"ReservationStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ReservationStatusToString(CReservationStatus Value) {
            switch (Value) {
                case rvsAccepted:
                    return "Accepted";
                case rvsFaulted:
                    return "Faulted";
                case rvsOccupied:
                    return "Occupied";
                case rvsRejected:
                    return "Rejected";
                case rvsUnavailable:
                    return "Unavailable";
                default:
                    throw ExceptionFrm("Invalid \"ReservationStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CCancelReservationStatus COCPPMessage::StringToCancelReservationStatus(const CString &Value) {
            if (Value == "Accepted")
                return crsAccepted;
            if (Value == "Rejected")
                return crsRejected;
            throw ExceptionFrm("Invalid \"CancelReservationStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::CancelReservationStatusToString(CCancelReservationStatus Value) {
            switch (Value) {
                case crsAccepted:
                    return "Accepted";
                case crsRejected:
                    return "Rejected";
                default:
                    throw ExceptionFrm("Invalid \"CancelReservationStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CClearCacheStatus COCPPMessage::StringToClearCacheStatus(const CString &Value) {
            if (Value == "Accepted")
                return ccsAccepted;
            if (Value == "Rejected")
                return ccsRejected;
            throw ExceptionFrm("Invalid \"ClearCacheStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ClearCacheStatusToString(CClearCacheStatus Value) {
            switch (Value) {
                case ccsAccepted:
                    return "Accepted";
                case ccsRejected:
                    return "Rejected";
                default:
                    throw ExceptionFrm("Invalid \"ClearCacheStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CTriggerMessageStatus COCPPMessage::StringToTriggerMessageStatus(const CString &Value) {
            if (Value == "Accepted")
                return tmsAccepted;
            if (Value == "Rejected")
                return tmsRejected;
            if (Value == "NotImplemented")
                return tmsNotImplemented;
            throw ExceptionFrm("Invalid \"TriggerMessageStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::TriggerMessageStatusToString(CTriggerMessageStatus Value) {
            switch (Value) {
                case tmsAccepted:
                    return "Accepted";
                case tmsRejected:
                    return "Rejected";
                case tmsNotImplemented:
                    return "NotImplemented";
                default:
                    throw ExceptionFrm("Invalid \"TriggerMessageStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CConfigurationStatus COCPPMessage::StringToConfigurationStatus(const CString &Value) {
            if (Value == "Accepted")
                return csAccepted;
            if (Value == "Rejected")
                return csRejected;
            if (Value == "RebootRequired")
                return csRebootRequired;
            if (Value == "NotSupported")
                return csNotSupported;
            throw ExceptionFrm("Invalid \"ConfigurationStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ConfigurationStatusToString(CConfigurationStatus Value) {
            switch (Value) {
                case csAccepted:
                    return "Accepted";
                case csRejected:
                    return "Rejected";
                case csRebootRequired:
                    return "RebootRequired";
                case csNotSupported:
                    return "NotSupported";
                default:
                    throw ExceptionFrm("Invalid \"ConfigurationStatus\" value: %d", Value);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargePointStatus COCPPMessage::StringToChargePointStatus(const CString &Value) {
            if (Value == "Available")
                return cpsAvailable;
            if (Value == "Preparing")
                return cpsPreparing;
            if (Value == "Charging")
                return cpsCharging;
            if (Value == "SuspendedEVSE")
                return cpsSuspendedEVSE;
            if (Value == "SuspendedEV")
                return cpsSuspendedEV;
            if (Value == "Finishing")
                return cpsFinishing;
            if (Value == "Reserved")
                return cpsReserved;
            if (Value == "Unavailable")
                return cpsUnavailable;
            if (Value == "Faulted")
                return cpsFaulted;
            throw ExceptionFrm("Invalid \"ChargePointStatus\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ChargePointStatusToString(CChargePointStatus Value) {
            switch (Value) {
                case cpsAvailable:
                    return "Available";
                case cpsPreparing:
                    return "Preparing";
                case cpsCharging:
                    return "Charging";
                case cpsSuspendedEVSE:
                    return "SuspendedEVSE";
                case cpsSuspendedEV:
                    return "SuspendedEV";
                case cpsFinishing:
                    return "Finishing";
                case cpsReserved:
                    return "Reserved";
                case cpsUnavailable:
                    return "Unavailable";
                case cpsFaulted:
                    return "Faulted";
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
            throw ExceptionFrm("Invalid \"ChargePointErrorCode\" value: %s", Value.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CString COCPPMessage::ChargePointErrorCodeToString(CChargePointErrorCode Value) {
            switch (Value) {
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

        bool COCPPMessage::Parse(CCSChargingPoint *APoint, const CSOAPMessage &Request, CSOAPMessage &Response) {

            const auto& action = Request.Notification.Lower();

            CSOAPProtocol::PrepareResponse(Request, Response);

            if (action == "authorizerequest") {
                APoint->Authorize(Request, Response);
            } else if (action == "bootnotificationrequest") {
                APoint->BootNotification(Request, Response);
            } else if (action == "datatransferrequest") {
                APoint->DataTransfer(Request, Response);
            } else if (action == "heartbeatrequest") {
                APoint->Heartbeat(Request, Response);
            } else if (action == "starttransactionrequest") {
                APoint->StartTransaction(Request, Response);
            } else if (action == "statusnotificationrequest") {
                APoint->StatusNotification(Request, Response);
            } else if (action == "stoptransactionrequest") {
                APoint->StopTransaction(Request, Response);
            } else {
                return false;
            }

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool COCPPMessage::Parse(CCSChargingPoint *APoint, const CJSONMessage &Request, CJSONMessage &Response) {

            const auto &action = Request.Action;

            CJSONProtocol::PrepareResponse(Request, Response);

            if (action == "Authorize") {
                APoint->Authorize(Request, Response);
            } else if (action == "BootNotification") {
                APoint->BootNotification(Request, Response);
            } else if (action == "DataTransfer") {
                APoint->DataTransfer(Request, Response);
            } else if (action == "Heartbeat") {
                APoint->Heartbeat(Request, Response);
            } else if (action == "MeterValues") {
                APoint->MeterValues(Request, Response);
            } else if (action == "StartTransaction") {
                APoint->StartTransaction(Request, Response);
            } else if (action == "StatusNotification") {
                APoint->StatusNotification(Request, Response);
            } else if (action == "StopTransaction") {
                APoint->StopTransaction(Request, Response);
            } else {
                return false;
            }

            return true;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessageHandler ---------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        COCPPMessageHandler::COCPPMessageHandler(COCPPMessageHandlerManager *AManager, const CJSONMessage &Message, COnMessageHandlerEvent &&Handler) :
                CCollectionItem(AManager), m_Handler(Handler) {
            m_Message = Message;
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPMessageHandler::Handler(CWebSocketConnection *AConnection) {
            if (m_Handler != nullptr) {
                m_Handler(this, AConnection);
            }
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessageHandlerManager --------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        COCPPMessageHandler *COCPPMessageHandlerManager::Get(int Index) const {
            return dynamic_cast<COCPPMessageHandler *> (inherited::GetItem(Index));
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPMessageHandlerManager::Set(int Index, COCPPMessageHandler *Value) {
            inherited::SetItem(Index, Value);
        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPMessageHandler *COCPPMessageHandlerManager::Send(const CJSONMessage &Message, COnMessageHandlerEvent &&Handler, uint32_t Key) {
            if (m_pChargingPoint->Connected()) {
                auto pHandler = new COCPPMessageHandler(this, Message, static_cast<COnMessageHandlerEvent &&> (Handler));

                CString sResult;
                CJSONProtocol::Response(Message, sResult);

                auto pWSReply = m_pChargingPoint->Connection()->WSReply();

                pWSReply->Clear();
                pWSReply->SetPayload(sResult, Key);

                m_pChargingPoint->Connection()->SendWebSocket(true);

                return pHandler;
            }

            return nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPMessageHandler *COCPPMessageHandlerManager::FindMessageById(const CString &Value) const {
            COCPPMessageHandler *pHandler;

            for (int i = 0; i < Count(); ++i) {
                pHandler = Get(i);
                if (pHandler->Message().UniqueId == Value)
                    return pHandler;
            }

            return nullptr;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CCustomChargingPoint --------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCustomChargingPoint::CCustomChargingPoint(): CObject() {
            m_ProtocolType = ptJSON;
            m_pConnection = nullptr;
            m_UpdateCount = 0;
            m_bUpdateConnected = false;
        }
        //--------------------------------------------------------------------------------------------------------------

        CCustomChargingPoint::CCustomChargingPoint(CWebSocketConnection *AConnection): CCustomChargingPoint() {
            m_pConnection = AConnection;
            AddToConnection(AConnection);
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCustomChargingPoint::Connected() {
            if (Assigned(m_pConnection)) {
                return m_pConnection->Connected();
            }
            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::AddToConnection(CWebSocketConnection *AConnection) {
            if (Assigned(AConnection)) {
                int Index = AConnection->Data().IndexOfName(CP_CONNECTION_DATA_NAME);
                if (Index == -1) {
                    AConnection->Data().AddObject(CP_CONNECTION_DATA_NAME, this);
                } else {
                    delete AConnection->Data().Objects(Index);
                    AConnection->Data().Objects(Index, this);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::DeleteFromConnection(CWebSocketConnection *AConnection) {
            if (Assigned(AConnection)) {
                int Index = AConnection->Data().IndexOfObject(this);
                if (Index != -1)
                    AConnection->Data().Delete(Index);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SwitchConnection(CWebSocketConnection *AConnection) {
            if (m_pConnection != AConnection) {
                BeginUpdate();

                if (Assigned(m_pConnection)) {
                    DeleteFromConnection(m_pConnection);
                    m_pConnection->Disconnect();
                    if (AConnection == nullptr)
                        delete m_pConnection;
                }

                if (Assigned(AConnection)) {
                    AddToConnection(AConnection);
                }

                m_pConnection = AConnection;

                EndUpdate();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CCustomChargingPoint *CCustomChargingPoint::FindOfConnection(CWebSocketConnection *AConnection) {
            int Index = AConnection->Data().IndexOfName(CP_CONNECTION_DATA_NAME);
            if (Index == -1)
                throw Delphi::Exception::ExceptionFrm("Not found charging point in connection");

            auto Object = AConnection->Data().Objects(Index);
            if (Object == nullptr)
                throw Delphi::Exception::ExceptionFrm("Object in connection data is null");

            auto Point = dynamic_cast<CCustomChargingPoint *> (Object);
            if (Point == nullptr)
                throw Delphi::Exception::Exception("Charging point is null");

            return Point;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SetProtocolType(CProtocolType Value) {
            if (Value != m_ProtocolType) {
                m_ProtocolType = Value;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SetUpdateConnected(bool Value) {
            if (Value != m_bUpdateConnected) {
              m_bUpdateConnected = Value;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SendMessage(const CJSONMessage &Message, bool ASendNow) {
            CString sResponse;
            DoMessageJSON(Message);
            CJSONProtocol::Response(Message, sResponse);
            chASSERT(m_pConnection);
            if (m_pConnection != nullptr && m_pConnection->Connected()) {
                m_pConnection->WSReply()->SetPayload(sResponse, (uint32_t) MsEpoch());
                m_pConnection->SendWebSocket(ASendNow);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SendMessage(const CJSONMessage &Message, COnMessageHandlerEvent &&Handler) {
            m_Messages.Send(Message, static_cast<COnMessageHandlerEvent &&> (Handler), (uint32_t) MsEpoch());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SendNotSupported(const CString &UniqueId, const CString &ErrorDescription, const CJSON &Payload) {
            SendMessage(CJSONProtocol::CallError(UniqueId, "NotSupported", ErrorDescription, Payload));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SendProtocolError(const CString &UniqueId, const CString &ErrorDescription, const CJSON &Payload) {
            SendMessage(CJSONProtocol::CallError(UniqueId, "ProtocolError", ErrorDescription, Payload));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::SendInternalError(const CString &UniqueId, const CString &ErrorDescription, const CJSON &Payload) {
            SendMessage(CJSONProtocol::CallError(UniqueId, "InternalError", ErrorDescription, Payload));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::DoMessageSOAP(const CSOAPMessage &Message) {
            if (m_OnMessageSOAP != nullptr) {
                m_OnMessageSOAP(this, Message);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomChargingPoint::DoMessageJSON(const CJSONMessage &Message) {
            if (m_OnMessageJSON != nullptr) {
                m_OnMessageJSON(this, Message);
            }
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CCharging -------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CChargingVoltage CCharging::StringToChargingVoltage(const CString &String) {
            if (String == "AC")
                return cvAC;
            if (String == "DC")
                return cvDC;
            return cvUnknown;
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingInterface CCharging::StringToChargingInterface(const CString &String) {
            if (String == "Schuko")
                return ciSchuko;
            if (String == "Type1")
                return ciType1;
            if (String == "Type2")
                return ciType2;
            if (String == "CCS1")
                return ciCCS1;
            if (String == "CCS2")
                return ciCCS2;
            if (String == "CHAdeMO")
                return ciCHAdeMO;
            if (String == "Commando")
                return ciCommando;
            if (String == "Tesla")
                return ciTesla;
            if (String == "SuperCharger")
                return ciSuperCharger;
            return ciUnknown;
        }
        //--------------------------------------------------------------------------------------------------------------

        CString CCharging::ChargingVoltageToString(CChargingVoltage voltage) {
            switch (voltage) {
                case cvAC:
                    return "AC";
                case cvDC:
                    return "DC";
                default:
                    return "Unknown";
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CString CCharging::ChargingInterfaceToString(CChargingInterface interface) {
            switch (interface) {
                case ciSchuko:
                    return "Schuko";
                case ciType1:
                    return "Type1";
                case ciType2:
                    return "Type2";
                case ciCCS1:
                    return "CCS1";
                case ciCCS2:
                    return "CCS2";
                case ciCHAdeMO:
                    return "CHAdeMO";
                case ciCommando:
                    return "Commando";
                case ciTesla:
                    return "Tesla";
                case ciSuperCharger:
                    return "SuperCharger";
                default:
                    return "Unknown";
            }
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPointConnector -----------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CChargingPointConnector::Clear() {
            m_ConnectorId = 0;
            m_TransactionId = -1;
            m_ReservationId = -1;
            m_MeterValue = 0;

            m_IdTag.Name().Clear();
            m_IdTag.Value().Clear();

            m_Charging.Clear();

            m_ErrorCode = cpeNoError;
            m_Status = cpsAvailable;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPointConnector::SetStatus(CChargePointStatus Value) {
            if (m_Status != Value) {
                m_Status = Value;
                UpdateStatusNotification();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPointConnector::IncMeterValue(int delta) {
            m_MeterValue += delta;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPointConnector::UpdateStatusNotification() {
            if (m_pChargingPoint != nullptr && m_pChargingPoint->Connected()) {
                m_pChargingPoint->SendStatusNotification(m_ConnectorId, m_Status, m_ErrorCode);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPointConnector::ContinueRemoteStartTransaction() {
            if (m_pChargingPoint != nullptr && m_pChargingPoint->Connected()) {

                auto OnRequest = [this](COCPPMessageHandler *AHandler, CWebSocketConnection *AWSConnection) {
                    const auto &message = CChargingPoint::RequestToMessage(AWSConnection);

                    if (message.Payload.HasOwnProperty("idTagInfo")) {
                        m_IdTag.Value() << message.Payload["idTagInfo"];
                        m_pChargingPoint->UpdateAuthorizationCache(m_IdTag);
                    }

                    if (m_IdTag.Value().status == asAccepted) {
                        if (!message.Payload.HasOwnProperty("transactionId")) {
                            throw Delphi::Exception::ExceptionFrm("Not found required key: %s", "transactionId");
                        }
                        m_TransactionId = message.Payload["transactionId"].AsInteger();
                        SetStatus(cpsCharging);
                    } else {
                        SetStatus(cpsPreparing);
                    }
                };

                m_pChargingPoint->SendStartTransaction(m_ConnectorId, m_IdTag.Name(), m_MeterValue, m_ReservationId, OnRequest);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CRemoteStartStopStatus CChargingPointConnector::RemoteStartTransaction(const CString &idTag) {
            if (m_Status != cpsAvailable || m_pChargingPoint == nullptr)
                return rssRejected;

            SetStatus(cpsPreparing);

            m_IdTag = m_pChargingPoint->AuthorizeLocal(idTag);

            auto OnRequest = [this](COCPPMessageHandler *AHandler, CWebSocketConnection *AWSConnection) {
                const auto &message = CChargingPoint::RequestToMessage(AWSConnection);
                if (message.Payload.HasOwnProperty("idTagInfo")) {
                    m_IdTag.Value() << message.Payload["idTagInfo"];
                    m_pChargingPoint->UpdateAuthorizationCache(m_IdTag);
                    if (m_IdTag.Value().status == asAccepted) {
                        ContinueRemoteStartTransaction();
                    } else {
                        SetStatus(cpsAvailable);
                    }
                }
            };

            if (m_IdTag.Value().status == asAccepted) {
                ContinueRemoteStartTransaction();
            } else {
                m_pChargingPoint->SendAuthorize(idTag, OnRequest);
            }

            return rssAccepted;
        }
        //--------------------------------------------------------------------------------------------------------------

        CRemoteStartStopStatus CChargingPointConnector::RemoteStopTransaction() {
            if (m_Status != cpsCharging || m_pChargingPoint == nullptr)
                return rssRejected;

            auto OnRequest = [this](COCPPMessageHandler *AHandler, CWebSocketConnection *AWSConnection) {
                const auto &message = CChargingPoint::RequestToMessage(AWSConnection);
                if (message.Payload.HasOwnProperty("idTagInfo")) {
                    m_IdTag.Value() << message.Payload["idTagInfo"];
                    m_pChargingPoint->UpdateAuthorizationCache(m_IdTag);
                }
            };

            if (m_ReservationId > 0 && m_IdTag.Name() != m_ReservationIdTag.Name()) {
                return rssRejected;
            }

            m_pChargingPoint->SendStopTransaction(m_IdTag.Name(), m_MeterValue, m_TransactionId, "Remote", OnRequest);

            m_TransactionId = -1;

            SetStatus(cpsFinishing);
            //SetStatus(cpsAvailable);

            return rssAccepted;
        }
        //--------------------------------------------------------------------------------------------------------------

        CReservationStatus CChargingPointConnector::ReserveNow(CDateTime expiryDate, const CString &idTag,
                const CString &parentIdTag, int reservationId) {

            if (reservationId <= 0 || m_Status == cpsFaulted)
                return rvsFaulted;

            if (m_Status == cpsUnavailable)
                return rvsUnavailable;

            if (reservationId == m_ReservationId) {
                if (m_Status != cpsReserved || m_ReservationIdTag.Name() != idTag)
                    return rvsOccupied;
            } else {
                if (m_Status != cpsAvailable)
                    return rvsOccupied;

                m_ReservationId = reservationId;

                m_ReservationIdTag.Name() = idTag;
                m_ReservationIdTag.Value().Clear();
            }

            m_ExpiryDate = expiryDate;

            SetStatus(cpsReserved);

            return rvsAccepted;
        }
        //--------------------------------------------------------------------------------------------------------------

        CCancelReservationStatus CChargingPointConnector::CancelReservation(int reservationId) {
            if (reservationId != m_ReservationId)
                return crsRejected;

            m_ReservationId = -1;
            m_ExpiryDate = 0;

            m_ReservationIdTag.Name().Clear();
            m_ReservationIdTag.Value().Clear();

            SetStatus(cpsAvailable);

            return crsAccepted;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CChargingPointConnector::MeterValues() {
            if ((m_Status == cpsCharging || m_Status == cpsSuspendedEVSE || m_Status == cpsSuspendedEV) && m_pChargingPoint != nullptr) {
                CJSONValue meterValue;
                CJSONArray meterArray;

                CJSONValue sampledValue;
                CJSONArray sampledArray;

                sampledValue.Object().AddPair("unit", "Wh");
                sampledValue.Object().AddPair("value", to_string(m_MeterValue));
                sampledValue.Object().AddPair("measurand", "Energy.Active.Import.Register");

                sampledArray.Add(sampledValue);

                meterValue.Object().AddPair("timestamp", GetISOTime());
                meterValue.Object().AddPair("sampledValue", sampledArray);

                meterArray.Add(meterValue);

                m_pChargingPoint->SendMeterValues(m_ConnectorId, m_TransactionId, meterArray);

                return true;
            }

            return false;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint::CChargingPoint(): CCustomChargingPoint() {
            m_BootNotificationStatus = rsPending;
            m_OnMessageSOAP = nullptr;
            m_OnMessageJSON = nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint::~CChargingPoint() {
            Save();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SetBootNotificationStatus(CRegistrationStatus Status) {
            if (m_BootNotificationStatus != Status) {
                m_BootNotificationStatus = Status;
                if (m_BootNotificationStatus == rsAccepted) {
                    StatusNotification(-1);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        int CChargingPoint::IndexOfConnectorId(int connectorId) const {
            for (int i = 0; i < m_Connectors.Count(); ++i) {
                if (m_Connectors[i].ConnectorId() == connectorId)
                    return i;
            }
            return -1;
        }
        //--------------------------------------------------------------------------------------------------------------

        int CChargingPoint::IndexOfTransactionId(int transactionId) const {
            for (int i = 0; i < m_Connectors.Count(); ++i) {
                if (m_Connectors[i].TransactionId() == transactionId)
                    return i;
            }
            return -1;
        }
        //--------------------------------------------------------------------------------------------------------------

        int CChargingPoint::IndexOfReservationId(int reservationId) const {
            for (int i = 0; i < m_Connectors.Count(); ++i) {
                if (m_Connectors[i].ReservationId() == reservationId)
                    return i;
            }
            return -1;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SetConnectors(const CJSON &connectors) {
            for (int i = 0; i < connectors.Count(); i++) {
                const auto &connector = connectors[i];
                CChargingPointConnector cpConnector(this, connector["id"].AsInteger(), connector["meterValue"].AsInteger());
                cpConnector.Status(cpsAvailable);
                cpConnector.Charging() = { CCharging::StringToChargingVoltage(connector["voltage"].AsString()), CCharging::StringToChargingInterface(connector["interface"].AsString()) };
                Connectors().Add(cpConnector);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SetConfigurationKeys(const CJSON &keys) {
            for (int i = 0; i < keys.Count(); ++i) {
                const auto &key = keys[i];
                const auto &name = key["key"].AsString();
                CConfigurationKey configurationKey(key);
                m_ConfigurationKeys.AddPair(name, configurationKey);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CJSONMessage CChargingPoint::RequestToMessage(CWebSocketConnection *AWSConnection) {
            auto pWSRequest = AWSConnection->WSRequest();
            CJSONMessage message;
            const CString payload(pWSRequest->Payload());
            CJSONProtocol::Request(payload, message);
            AWSConnection->ConnectionStatus(csReplySent);
            pWSRequest->Clear();
            return message;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Ping() {
            if (Assigned(m_pConnection)) {
                if (m_pConnection->Connected()) {
                    m_pConnection->SendWebSocketPing(true);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Save() {
            Save(m_Prefix + "configuration.json");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Reload() {
            m_Connectors.Clear();
            m_ConfigurationKeys.Clear();
            Load(m_Prefix + "configuration.json");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Load(const CString &FileName) {
            CJSON json;
            json.LoadFromFile(FileName.c_str());

            SetConnectors(json["connectorId"].AsArray());
            SetConfigurationKeys(json["configurationKey"].AsArray());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Save(const CString &FileName) {
            CJSON json;

            const auto &connectorId = ConnectorToJson();
            const auto &configurationKey = ConfigurationToJson();

            json.Object().AddPair("connectorId", connectorId["connectorId"].AsArray());
            json.Object().AddPair("configurationKey", configurationKey["configurationKey"].AsArray());

            json.SaveToFile(FileName.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CIdTag CChargingPoint::AuthorizeLocal(const CString &idTag) {
            CIdTag IdTag;
            IdTag.Name() = idTag;

            if (m_ConfigurationKeys["AuthorizeRemoteTxRequests"].value == "true") {
                if (m_ConfigurationKeys["AuthorizationCacheEnabled"].value == "true" ) {
                    const auto index = m_AuthorizationCache.IndexOfName(idTag);
                    if (index != -1) {
                        IdTag = m_AuthorizationCache[index];
                        if (IdTag.Value().expiryDate <= UTC()) {
                            IdTag.Value().status = asExpired;
                        }
                    }
                }
            } else {
                IdTag.Value().expiryDate = 0;
                IdTag.Value().parentIdTag.Clear();
                IdTag.Value().status = asAccepted;
            }

            return IdTag;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::UpdateAuthorizationCache(const CIdTag &IdTag) {
            if (m_ConfigurationKeys["AuthorizationCacheEnabled"].value == "true" ) {
                const auto index = m_AuthorizationCache.IndexOfName(IdTag.Name());
                if (index == -1) {
                    m_AuthorizationCache.Add(IdTag);
                } else {
                    m_AuthorizationCache[index] = IdTag;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CChargingPoint::MeterValues(int connectorId) {
            bool result;

            if (connectorId == -1) {
                for (int i = 0; i < Connectors().Count(); ++i) {
                    result |= Connectors()[i].MeterValues();
                }
            } else if (connectorId == 0) {
                result = m_ConnectorZero.MeterValues();
            } else {
                const auto index = IndexOfConnectorId(connectorId);
                if (index == -1) {
                    throw Delphi::Exception::ExceptionFrm(CP_INVALID_CONNECTION_ID, connectorId);
                }
                result = Connectors()[index].MeterValues();
            }

            return result;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StatusNotification(int connectorId) {
            if (connectorId == -1) {
                for (int i = 0; i < Connectors().Count(); ++i) {
                    Connectors()[i].UpdateStatusNotification();
                }
            } else if (connectorId == 0) {
                m_ConnectorZero.UpdateStatusNotification();
            } else {
                const auto index = IndexOfConnectorId(connectorId);
                if (index == -1) {
                    throw Delphi::Exception::ExceptionFrm(CP_INVALID_CONNECTION_ID, connectorId);
                }
                Connectors()[index].UpdateStatusNotification();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CCancelReservationStatus CChargingPoint::CancelReservation(int reservationId) {
            int index = IndexOfReservationId(reservationId);
            if (index == -1) {
                return crsRejected;
            }

            return m_Connectors[index].CancelReservation(reservationId);
        }
        //--------------------------------------------------------------------------------------------------------------

        CConfigurationStatus CChargingPoint::ChangeConfiguration(const CString &key, const CString &value) {
            const auto index = m_ConfigurationKeys.IndexOfName(key);

            if (index != -1) {
                auto &configurationKey = m_ConfigurationKeys[index].Value();
                if (configurationKey.readonly) {
                    return csRejected;
                }

                configurationKey.value = value;

                Save();

                if (value == "CentralSystemURL")
                    return csRebootRequired;

                if (value == "HeartbeatInterval")
                    return csRebootRequired;

                return csAccepted;
            }

            return csNotSupported;
        }
        //--------------------------------------------------------------------------------------------------------------

        CClearCacheStatus CChargingPoint::ClearCache() {
            if (m_ConfigurationKeys["AuthorizationCacheEnabled"].value == "true" ) {
                m_AuthorizationCache.Clear();
                return ccsAccepted;
            }
            return ccsRejected;
        }
        //--------------------------------------------------------------------------------------------------------------

        CJSON CChargingPoint::ConnectorToJson(int connectorId) {
            CJSON json;
            CJSONArray jsonArray;

            auto add_connector = [&jsonArray](const CChargingPointConnector &connector) {
                CJSONValue value;

                value.Object().AddPair("id", connector.ConnectorId());
                value.Object().AddPair("voltage", CCharging::ChargingVoltageToString(connector.Charging().voltage));
                value.Object().AddPair("interface", CCharging::ChargingInterfaceToString(connector.Charging().interface));
                value.Object().AddPair("meterValue", connector.MeterValue());

                jsonArray.Add(value);
            };

            if (connectorId == -1) {
                for (int i = 0; i < Connectors().Count(); ++i) {
                    add_connector(Connectors()[i]);
                }
            } else if (connectorId == 0) {
                add_connector(m_ConnectorZero);
            } else {
                const auto index = IndexOfConnectorId(connectorId);
                if (index == -1) {
                    throw Delphi::Exception::ExceptionFrm(CP_INVALID_CONNECTION_ID, connectorId);
                }
                add_connector(Connectors()[index]);
            }

            json.Object().AddPair("connectorId", jsonArray);

            return json;
        }
        //--------------------------------------------------------------------------------------------------------------

        CJSON CChargingPoint::ConfigurationToJson(const CString &key) {
            CJSON json;
            CJSONArray jsonArray;

            auto add_key = [&jsonArray](const CConfigurationKey &configurationKey) {
                CJSONValue value;

                value.Object().AddPair("key", configurationKey.key);
                value.Object().AddPair("readonly", configurationKey.readonly);
                value.Object().AddPair("value", configurationKey.value);

                jsonArray.Add(value);
            };

            if (!key.empty()) {
                add_key(m_ConfigurationKeys[key]);
            } else {
                for (int i = 0; i < m_ConfigurationKeys.Count(); ++i) {
                    add_key(m_ConfigurationKeys[i].Value());
                }
            }

            json.Object().AddPair("configurationKey", jsonArray);

            return json;
        }
        //--------------------------------------------------------------------------------------------------------------

        CRemoteStartStopStatus CChargingPoint::RemoteStartTransaction(const CString &idTag, int connectorId, const CJSON &chargingProfile) {
            int index = 0;

            if (connectorId > 0) {
                index = IndexOfConnectorId(connectorId);
                if (index == -1) {
                    throw Delphi::Exception::ExceptionFrm(CP_INVALID_CONNECTION_ID, connectorId);
                }
            } else {
                while (index < m_Connectors.Count() && m_Connectors[index].Status() != cpsAvailable)
                    index++;
                if (index == m_Connectors.Count())
                    return rssRejected;
            }

            return m_Connectors[index].RemoteStartTransaction(idTag);
        }
        //--------------------------------------------------------------------------------------------------------------

        CRemoteStartStopStatus CChargingPoint::RemoteStopTransaction(int transactionId) {
            int index = IndexOfTransactionId(transactionId);
            if (index == -1) {
                return rssRejected;
            }

            return m_Connectors[index].RemoteStopTransaction();
        }
        //--------------------------------------------------------------------------------------------------------------

        CReservationStatus CChargingPoint::ReserveNow(int connectorId, CDateTime expiryDate, const CString &idTag,
                const CString &parentIdTag, int reservationId) {

            int index = 0;

            if (connectorId == -1) {
                return rvsRejected;
            } else if (connectorId == 0) {
                if (m_ConfigurationKeys["ReserveConnectorZeroSupported"].value != "true") {
                    return rvsRejected;
                }
                return m_ConnectorZero.ReserveNow(expiryDate, idTag, parentIdTag, reservationId);
            } else if (connectorId > 0) {
                index = IndexOfConnectorId(connectorId);
                if (index == -1) {
                    throw Delphi::Exception::ExceptionFrm(CP_INVALID_CONNECTION_ID, connectorId);
                }
            }

            return m_Connectors[index].ReserveNow(expiryDate, idTag, parentIdTag, reservationId);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Reset() {
            Save();
        }
        //--------------------------------------------------------------------------------------------------------------

        CTriggerMessageStatus CChargingPoint::TriggerMessage(CMessageTrigger requestedMessage, int connectorId) {
            switch (requestedMessage) {
                case mtBootNotification:
                    SendBootNotification();
                    return tmsAccepted;
                case mtHeartbeat:
                    SendHeartbeat();
                    return tmsAccepted;
                case mtStatusNotification:
                    StatusNotification(connectorId);
                    return tmsAccepted;
                case mtMeterValues:
                    MeterValues(connectorId);
                    return tmsAccepted;
                case mtDiagnosticsStatusNotification:
                case mtFirmwareStatusNotification:
                default:
                    return tmsNotImplemented;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SendAuthorize(const CString &idTag, COnMessageHandlerEvent &&OnRequest) {
            CJSONMessage message;

            message.MessageTypeId = ChargePoint::mtCall;
            message.UniqueId = GenUniqueId();
            message.Action = "Authorize";

            message.Payload.Object().AddPair("idTag", idTag);

            SendMessage(message, std::move(OnRequest));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SendBootNotification() {

            auto OnRequest = [this](COCPPMessageHandler *AHandler, CWebSocketConnection *AWSConnection) {
                const auto &message = RequestToMessage(AWSConnection);
                if (message.Payload.HasOwnProperty("status")) {
                    SetBootNotificationStatus(COCPPMessage::StringToRegistrationStatus(message.Payload["status"].AsString()));
                }
            };

            CJSONMessage message;

            message.MessageTypeId = ChargePoint::mtCall;
            message.UniqueId = GenUniqueId();
            message.Action = "BootNotification";

            message.Payload.Object().AddPair("chargePointModel", m_ConfigurationKeys["ChargePointModel"].value);
            message.Payload.Object().AddPair("chargePointVendor", m_ConfigurationKeys["ChargePointVendor"].value);
            message.Payload.Object().AddPair("chargePointSerialNumber", m_ConfigurationKeys["ChargePointSerialNumber"].value);
            message.Payload.Object().AddPair("firmwareVersion", m_ConfigurationKeys["ChargePointSoftwareVersion"].value);

            SendMessage(message, OnRequest);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SendHeartbeat() {
            CJSONMessage message;

            message.MessageTypeId = ChargePoint::mtCall;
            message.UniqueId = GenUniqueId();
            message.Action = "Heartbeat";

            SendMessage(message, true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SendMeterValues(int connectorId, int transactionId, const CJSONArray &meterValue) {
            CJSONMessage message;

            message.MessageTypeId = ChargePoint::mtCall;
            message.UniqueId = GenUniqueId();
            message.Action = "MeterValues";

            message.Payload.Object().AddPair("connectorId", connectorId);

            if (transactionId > 0) {
                message.Payload.Object().AddPair("transactionId", transactionId);
            }

            message.Payload.Object().AddPair("meterValue", meterValue);

            SendMessage(message, true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SendStartTransaction(int connectorId, const CString &idTag, int meterStart, int reservationId, COnMessageHandlerEvent &&OnRequest) {
            CJSONMessage message;

            message.MessageTypeId = ChargePoint::mtCall;
            message.UniqueId = GenUniqueId();
            message.Action = "StartTransaction";

            message.Payload.Object().AddPair("connectorId", connectorId);
            message.Payload.Object().AddPair("idTag", idTag);
            message.Payload.Object().AddPair("meterStart", meterStart);

            if (reservationId >= 0) {
                message.Payload.Object().AddPair("reservationId", reservationId);
            }

            message.Payload.Object().AddPair("timestamp", GetISOTime());

            SendMessage(message, std::move(OnRequest));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SendStatusNotification(int connectorId, CChargePointStatus status, CChargePointErrorCode errorCode) {
            CJSONMessage message;

            message.MessageTypeId = ChargePoint::mtCall;
            message.UniqueId = GenUniqueId();
            message.Action = "StatusNotification";

            message.Payload.Object().AddPair("status", COCPPMessage::ChargePointStatusToString(status));
            message.Payload.Object().AddPair("errorCode", COCPPMessage::ChargePointErrorCodeToString(errorCode));
            message.Payload.Object().AddPair("timestamp", GetISOTime());
            message.Payload.Object().AddPair("connectorId", connectorId);

            SendMessage(message, true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::SendStopTransaction(const CString &idTag, int meterStop, int transactionId,
                const CString &reason, COnMessageHandlerEvent &&OnRequest) {

            CJSONMessage message;

            message.MessageTypeId = ChargePoint::mtCall;
            message.UniqueId = GenUniqueId();
            message.Action = "StopTransaction";

            message.Payload.Object().AddPair("idTag", idTag);
            message.Payload.Object().AddPair("meterStop", meterStop);
            message.Payload.Object().AddPair("timestamp", GetISOTime());
            message.Payload.Object().AddPair("transactionId", transactionId);

            if (!reason.empty()) {
                message.Payload.Object().AddPair("reason", reason);
            }

            SendMessage(message, std::move(OnRequest));
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSChargingPoint ------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCSChargingPoint::CCSChargingPoint(CCSChargingPointManager *AManager, CWebSocketConnection *AConnection):
            CCollectionItem(AManager), CCustomChargingPoint(AConnection) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::Authorize(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_AuthorizeRequest << Body;

            Response.Notification = "authorizeResponse";

            CString idTagInfo(LINEFEED);
            CStringPairs Values;

            Values.AddPair("status", COCPPMessage::AuthorizationStatusToString(asAccepted));
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name();
                const CString& Value = Values[i].Value();

                idTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", idTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::Authorize(const CJSONMessage &Request, CJSONMessage &Response) {

            m_AuthorizeRequest << Request.Payload;

            CJSONValue idTagInfo(jvtObject);

            idTagInfo.Object().AddPair("status", COCPPMessage::AuthorizationStatusToString(asAccepted));
            idTagInfo.Object().AddPair("expiryDate", GetISOTime(5 * 60));

            auto& Values = Response.Payload.Object();
            Values.AddPair("idTagInfo", idTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::StartTransaction(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StartTransactionRequest << Body;

            Response.Notification = "startTransactionResponse";

            CString idTagInfo(LINEFEED);
            CStringPairs Values;

            Response.Values.AddPair("transactionId", IntToString(++nTransactionId));

            Values.AddPair("status", COCPPMessage::AuthorizationStatusToString(asAccepted));
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name();
                const CString& Value = Values[i].Value();

                idTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", idTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::StartTransaction(const CJSONMessage &Request, CJSONMessage &Response) {

            CJSONValue idTagInfo(jvtObject);

            idTagInfo.Object().AddPair("status", COCPPMessage::AuthorizationStatusToString(asAccepted));
            idTagInfo.Object().AddPair("expiryDate", GetISOTime(5 * 60));

            auto& Values = Response.Payload.Object();

            Values.AddPair("idTagInfo", idTagInfo);
            Values.AddPair("transactionId", IntToString(++nTransactionId));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::StopTransaction(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const auto &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StopTransactionRequest << Body;

            Response.Notification = "stopTransactionResponse";

            CString idTagInfo(LINEFEED);
            CStringPairs Values;

            Values.AddPair("status", COCPPMessage::AuthorizationStatusToString(asAccepted));
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name();
                const CString& Value = Values[i].Value();

                idTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", idTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::StopTransaction(const CJSONMessage &Request, CJSONMessage &Response) {
            CJSONValue idTagInfo(jvtObject);

            idTagInfo.Object().AddPair("status", COCPPMessage::AuthorizationStatusToString(asAccepted));
            idTagInfo.Object().AddPair("expiryDate", GetISOTime(5 * 60));

            auto& Values = Response.Payload.Object();

            Values.AddPair("idTagInfo", idTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::BootNotification(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const auto &Body = Request.Values;

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

        void CCSChargingPoint::BootNotification(const CJSONMessage &Request, CJSONMessage &Response) {

            m_BootNotificationRequest << Request.Payload;

            Response.MessageTypeId = mtCallResult;

            auto& Values = Response.Payload.Object();

            Values.AddPair("status", "Accepted");
            Values.AddPair("currentTime", GetISOTime());
            Values.AddPair("interval", 60);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::StatusNotification(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const auto &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StatusNotificationRequest << Body;

            Response.Notification = "statusNotificationResponse";
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::StatusNotification(const CJSONMessage &Request, CJSONMessage &Response) {
            m_StatusNotificationRequest << Request.Payload;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::DataTransfer(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const auto &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_DataTransferRequest << Body;

            Response.Notification = "dataTransferResponse";

            auto& Values = Response.Values;
            Values.AddPair("status", "Accepted");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::DataTransfer(const CJSONMessage &Request, CJSONMessage &Response) {
            m_DataTransferRequest << Request.Payload;

            auto& Values = Response.Payload.Object();
            Values.AddPair("status", "Accepted");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::Heartbeat(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            Response.Notification = "heartbeatResponse";

            auto& Values = Response.Values;
            Values.AddPair("currentTime", GetISOTime());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::MeterValues(const CSOAPMessage &Request, CSOAPMessage &Response) {
            const auto &Headers = Request.Headers;
            const auto &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_MeterValuesRequest << Body;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::MeterValues(const CJSONMessage &Request, CJSONMessage &Response) {
            m_MeterValuesRequest << Request.Payload;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPoint::Heartbeat(const CJSONMessage &Request, CJSONMessage &Response) {
            auto& Values = Response.Payload.Object();
            Values.AddPair("currentTime", GetISOTime());
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSChargingPoint::ParseSOAP(const CString &Request, CString &Response) {
            CSOAPMessage jmRequest;
            CSOAPMessage jmResponse;

            CSOAPProtocol::Request(Request, jmRequest);
            if (COCPPMessage::Parse(this, jmRequest, jmResponse)) {
                DoMessageSOAP(jmRequest);
                CSOAPProtocol::Response(jmResponse, Response);
                return true;
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSChargingPoint::ParseJSON(const CString &Request, CString &Response) {
            CJSONMessage jmRequest;
            CJSONMessage jmResponse;

            CJSONProtocol::Request(Request, jmRequest);
            if (jmRequest.MessageTypeId == mtCall) {
                if (COCPPMessage::Parse(this, jmRequest, jmResponse)) {
                    DoMessageJSON(jmRequest);
                    DoMessageJSON(jmResponse);
                    CJSONProtocol::Response(jmResponse, Response);
                    return true;
                }
            } else {
                auto pHandler = m_Messages.FindMessageById(jmRequest.UniqueId);
                if (Assigned(pHandler)) {
                    jmRequest.Action = pHandler->Message().Action;
                    DoMessageJSON(jmRequest);
                    pHandler->Handler(m_pConnection);
                    delete pHandler;
                    return true;
                }
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSChargingPoint::Parse(const CString &Request, CString &Response) {
            switch (m_ProtocolType) {
                case ptSOAP:
                    return ParseSOAP(Request, Response);
                case ptJSON:
                    return ParseJSON(Request, Response);
            }
            return false;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSChargingPointManager -------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCSChargingPoint *CCSChargingPointManager::Get(int Index) const {
            return dynamic_cast<CCSChargingPoint *> (inherited::GetItem(Index));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSChargingPointManager::Set(int Index, CCSChargingPoint *Value) {
            inherited::SetItem(Index, Value);
        }
        //--------------------------------------------------------------------------------------------------------------

        CCSChargingPoint *CCSChargingPointManager::Add(CWebSocketConnection *AConnection) {
            return new CCSChargingPoint(this, AConnection);
        }
        //--------------------------------------------------------------------------------------------------------------

        CCSChargingPoint *CCSChargingPointManager::FindPointByIdentity(const CString &Value) {
            for (int i = 0; i < Count(); ++i) {
                auto pPoint = Get(i);
                if (pPoint->Identity() == Value)
                    return pPoint;
            }

            return nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        CCSChargingPoint *CCSChargingPointManager::FindPointByConnection(CWebSocketConnection *Value) {
            for (int i = 0; i < Count(); ++i) {
                auto pPoint = Get(i);
                if (pPoint->Connection() == Value)
                    return pPoint;
            }

            return nullptr;
        }

    }
}
}
