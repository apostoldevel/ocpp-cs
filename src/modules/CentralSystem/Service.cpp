/*++

Program name:

  Apostol CSService

Module Name:

  CSService.cpp

Notices:

  Module CSService

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

//----------------------------------------------------------------------------------------------------------------------

#include "Core.hpp"
#include "Service.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include <openssl/sha.h>
//----------------------------------------------------------------------------------------------------------------------

#include "rapidxml.hpp"

using namespace rapidxml;
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace CSService {

        CString SHA1(const CString &data) {
            CString digest;
            digest.SetLength(SHA_DIGEST_LENGTH);
            ::SHA1((unsigned char *) data.data(), data.length(), (unsigned char *) digest.Data());
            return digest;
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

        //-- CSOAPProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::Request(const CString &xmlString, CMessage &Message) {
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

        void CSOAPProtocol::Response(const CMessage &Message, CString &xmlString) {

            const CStringPairs &Headers = Message.Headers;
            const CStringPairs &Values = Message.Values;

            xmlString =  R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED;
            xmlString << R"(<s:Envelope xmlns:a="http://www.w3.org/2005/08/addressing")" LINEFEED;
            xmlString << R"(xmlns:s="http://www.w3.org/2003/05/soap-envelope")" LINEFEED;
            xmlString << R"(xmlns="urn://Ocpp/Cs/2012/06/">)" LINEFEED;

            xmlString << R"(<s:Header>)" LINEFEED;

            for (int i = 0; i < Headers.Count(); i++ ) {
                const CString& Name = Headers[i].Name;
                const CString& Value = Headers[i].Value;

                if (Name == "a:From" || Name == "a:ReplyTo" || Name == "a:FaultTo")
                    xmlString.Format(R"(<%s><a:Address>%s</a:Address></%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
                else
                    xmlString.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            xmlString << R"(</s:Header>)" LINEFEED;
            xmlString << R"(<s:Body>)" LINEFEED;

            xmlString.Format(R"(<%s>)" LINEFEED, Message.Notification.c_str());

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name;
                const CString& Value = Values[i].Value;

                xmlString.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            xmlString.Format(R"(</%s>)" LINEFEED, Message.Notification.c_str());

            xmlString << R"(</s:Body>)" LINEFEED;
            xmlString << R"(</s:Envelope>)";
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::PrepareResponse(const CMessage &Request, CMessage &Response) {

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
            Response.Headers.AddPair("a:Action", Action);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CChargePointStatus CSOAPMessage::StringToChargePointStatus(const CString &Value) {
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

        CString CSOAPMessage::ChargePointStatusToString(CChargePointStatus Value) {
            switch (Value) {
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

        CChargePointErrorCode CSOAPMessage::StringToChargePointErrorCode(const CString &Value) {
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

        CString CSOAPMessage::ChargePointErrorCodeToString(CChargePointErrorCode Value) {
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

        void CSOAPMessage::Parse(CChargingPoint *APoint, const CMessage &Request, CMessage &Response) {

            CSOAPProtocol::PrepareResponse(Request, Response);

            if (Request.Notification.Lower() == "authorizerequest") {
                APoint->Authorize(Request, Response);
            } else if (Request.Notification.Lower() == "starttransactionrequest") {
                APoint->StartTransaction(Request, Response);
            } else if (Request.Notification.Lower() == "stoptransactionrequest") {
                APoint->StopTransaction(Request, Response);
            } else if (Request.Notification.Lower() == "bootnotificationrequest") {
                APoint->BootNotification(Request, Response);
            } else if (Request.Notification.Lower() == "statusnotificationrequest") {
                APoint->StatusNotification(Request, Response);
            } else if (Request.Notification.Lower() == "heartbeatrequest") {
                APoint->Heartbeat(Request, Response);
            }
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint::CChargingPoint(CHTTPServerConnection *AConnection, CChargingPointManager *AManager) : CCollectionItem(AManager) {
            m_Connection = AConnection;
            m_TransactionId = 0;
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint::~CChargingPoint() {
            m_Connection = nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StringToMeterValue(const CString &String, CMeterValue &MeterValue) {
            xml_document<> xmlDocument;
            xmlDocument.parse<0>((char *) String.c_str());

            xml_node<> *meterValue = xmlDocument.first_node("MeterValue");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Authorize(const CMessage &Request, CMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_AuthorizeRequest.idTag = Body.Values("idTag");

            Response.Notification = "authorizeResponse";

            CString IdTagInfo(LINEFEED);
            CStringPairs Values;

            Values.AddPair("status", "Accepted");
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name;
                const CString& Value = Values[i].Value;

                IdTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StartTransaction(const CMessage &Request, CMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StartTransactionRequest.connectorId = StrToInt(Body.Values("connectorId").c_str());
            m_StartTransactionRequest.idTag = Body.Values("idTag");
            m_StartTransactionRequest.timestamp = StrToDateTimeDef(Body.Values("timestamp").c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");
            m_StartTransactionRequest.meterStart = StrToInt(Body.Values("meterStart").c_str());
            m_StartTransactionRequest.reservationId = StrToInt(Body.Values("reservationId").c_str());

            Response.Notification = "startTransactionResponse";

            CString IdTagInfo(LINEFEED);
            CStringPairs Values;

            Response.Values.AddPair("transactionId", IntToString(++m_TransactionId));

            Values.AddPair("status", "Accepted");
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name;
                const CString& Value = Values[i].Value;

                IdTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StopTransaction(const CMessage &Request, CMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StopTransactionRequest.transactionId = StrToInt(Body.Values("transactionId").c_str());
            m_StopTransactionRequest.idTag = Body.Values("idTag");
            m_StopTransactionRequest.timestamp = StrToDateTimeDef(Body.Values("timestamp").c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");
            m_StopTransactionRequest.meterStop = StrToInt(Body.Values("meterStop").c_str());
            m_StopTransactionRequest.reason = Body.Values("reason");

            StringToMeterValue(Body.Values("transactionData"), m_StopTransactionRequest.transactionData);

            Response.Notification = "stopTransactionResponse";

            CString IdTagInfo(LINEFEED);
            CStringPairs Values;

            Values.AddPair("status", "Accepted");
            Values.AddPair("expiryDate", GetISOTime(5 * 60));

            for (int i = 0; i < Values.Count(); i++ ) {
                const CString& Name = Values[i].Name;
                const CString& Value = Values[i].Value;

                IdTagInfo.Format(R"(<%s>%s</%s>)" LINEFEED, Name.c_str(), Value.c_str(), Name.c_str());
            }

            Response.Values.AddPair("idTagInfo", IdTagInfo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::BootNotification(const CMessage &Request, CMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_BootNotificationRequest.iccid = Body.Values("iccid");
            m_BootNotificationRequest.chargePointVendor = Body.Values("chargePointVendor");
            m_BootNotificationRequest.chargePointModel = Body.Values("chargePointModel");
            m_BootNotificationRequest.chargePointSerialNumber = Body.Values("chargePointSerialNumber");
            m_BootNotificationRequest.chargeBoxSerialNumber = Body.Values("chargeBoxSerialNumber");
            m_BootNotificationRequest.firmwareVersion = Body.Values("firmwareVersion");
            m_BootNotificationRequest.meterType = Body.Values("meterType");
            m_BootNotificationRequest.meterSerialNumber = Body.Values("meterSerialNumber");

            Response.Notification = "bootNotificationResponse";

            Response.Values.AddPair("status", "Accepted");
            Response.Values.AddPair("currentTime", GetISOTime());
            Response.Values.AddPair("heartbeatInterval", "60");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::StatusNotification(const CMessage &Request, CMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            m_StatusNotificationRequest.connectorId = StrToInt(Body.Values("connectorId").c_str());
            m_StatusNotificationRequest.status = CSOAPMessage::StringToChargePointStatus(Body.Values("status"));
            m_StatusNotificationRequest.errorCode = CSOAPMessage::StringToChargePointErrorCode(Body.Values("errorCode"));
            m_StatusNotificationRequest.info = Body.Values("info");
            m_StatusNotificationRequest.timestamp = StrToDateTimeDef(Body.Values("timestamp").c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");
            m_StatusNotificationRequest.vendorId = Body.Values("vendorId");
            m_StatusNotificationRequest.vendorErrorCode = Body.Values("vendorErrorCode");

            Response.Notification = "statusNotificationResponse";
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Heartbeat(const CMessage &Request, CMessage &Response) {
            const CStringPairs &Headers = Request.Headers;
            const CStringPairs &Body = Request.Values;

            m_Address = Headers.Values("a:From");
            m_Identity = Headers.Values("chargeBoxIdentity");

            Response.Notification = "heartbeatResponse";
            Response.Values.AddPair("currentTime", GetISOTime());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CChargingPoint::Parse(const CString &Request, CString &Response) {

            CMessage LRequest;
            CMessage LResponse;

            CSOAPProtocol::Request(Request, LRequest);
            CSOAPMessage::Parse(this, LRequest, LResponse);
            CSOAPProtocol::Response(LResponse, Response);
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
                    break;
            }
            return Point;
        }
        //--------------------------------------------------------------------------------------------------------------

        CChargingPoint *CChargingPointManager::FindPointByConnection(CHTTPServerConnection *Value) {
            CChargingPoint *Point = nullptr;
            for (int I = 0; I < Count(); ++I) {
                Point = Get(I);
                if (Point->Connection() == Value)
                    break;
            }
            return Point;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCSService::CCSService(CModuleManager *AManager) : CApostolModule(AManager) {
            m_CPManager = new CChargingPointManager();
            InitMethods();
        }
        //--------------------------------------------------------------------------------------------------------------

        CCSService::~CCSService() {
            delete m_CPManager;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::InitMethods() {
            m_Methods.AddObject(_T("GET"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoGet, this, _1)));
            m_Methods.AddObject(_T("POST"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoPost, this, _1)));
            m_Methods.AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoOptions, this, _1)));
            m_Methods.AddObject(_T("PUT"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("DELETE"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("TRACE"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("HEAD"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("PATCH"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DebugRequest(CRequest *ARequest) {
            DebugMessage("[%p] Request:\n%s %s HTTP/%d.%d\n", ARequest, ARequest->Method.c_str(), ARequest->Uri.c_str(), ARequest->VMajor, ARequest->VMinor);

            for (int i = 0; i < ARequest->Headers.Count(); i++)
                DebugMessage("%s: %s\n", ARequest->Headers[i].Name.c_str(), ARequest->Headers[i].Value.c_str());

            DebugMessage("\n%s\n", ARequest->Content.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DebugReply(CReply *AReply) {
            CMemoryStream Stream;
            AReply->ToBuffers(&Stream);
            CString S(&Stream);
            DebugMessage("[%p] Reply:\n%s\n", AReply, S.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::ExceptionToJson(int ErrorCode, Delphi::Exception::Exception *AException, CString& Json) {

            TCHAR ch;
            LPCTSTR lpMessage = AException->what();
            CString Message;

            while ((ch = *lpMessage++) != 0) {
                if ((ch == '"') || (ch == '\\'))
                    Message.Append('\\');
                Message.Append(ch);
            }

            Json.Format(R"({"error": {"code": %u, "message": "%s"}})", ErrorCode, Message.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->PollConnection());

            if (LConnection != nullptr) {
                auto LReply = LConnection->Reply();

                CReply::status_type LStatus = CReply::internal_server_error;

                ExceptionToJson(0, AException, LReply->Content);

                LConnection->SendReply(LStatus);
            }

            Log()->Error(APP_LOG_EMERG, 0, AException->what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::PQResultToJson(CPQResult *Result, CString &Json) {
            Json = "{";

            for (int I = 0; I < Result->nFields(); ++I) {
                if (I > 0) {
                    Json += ", ";
                }

                Json += "\"";
                Json += Result->fName(I);
                Json += "\"";

                if (SameText(Result->fName(I),_T("session"))) {
                    Json += ": ";
                    if (Result->GetIsNull(0, I)) {
                        Json += _T("null");
                    } else {
                        Json += "\"";
                        Json += Result->GetValue(0, I);
                        Json += "\"";
                    }
                } else if (SameText(Result->fName(I),_T("result"))) {
                    Json += ": ";
                    if (SameText(Result->GetValue(0, I), _T("t"))) {
                        Json += _T("true");
                    } else {
                        Json += _T("false");
                    }
                } else {
                    Json += ": \"";
                    Json += Result->GetValue(0, I);
                    Json += "\"";
                }
            }

            Json += "}";
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::QueryToJson(CPQPollQuery *Query, CString& Json) {

            CPQResult *Run;
            CPQResult *Login;
            CPQResult *Result;

            for (int I = 0; I < Query->Count(); I++) {
                Result = Query->Results(I);
                if (Result->ExecStatus() != PGRES_TUPLES_OK)
                    throw Delphi::Exception::EDBError(Result->GetErrorMessage());
            }

            if (Query->Count() == 3) {
                Login = Query->Results(0);

                if (SameText(Login->GetValue(0, 1), "f")) {
                    Log()->Error(APP_LOG_EMERG, 0, Login->GetValue(0, 2));
                    PQResultToJson(Login, Json);
                    return;
                }

                Run = Query->Results(1);
            } else {
                Run = Query->Results(0);
            }

            Json = "{\"result\": ";

            if (Run->nTuples() > 0) {

                Json += "[";
                for (int Row = 0; Row < Run->nTuples(); ++Row) {
                    for (int Col = 0; Col < Run->nFields(); ++Col) {
                        if (Row != 0)
                            Json += ", ";
                        if (Run->GetIsNull(Row, Col)) {
                            Json += "null";
                        } else {
                            Json += Run->GetValue(Row, Col);
                        }
                    }
                }
                Json += "]";

            } else {
                Json += "{}";
            }

            Json += "}";
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPostgresQueryExecuted(CPQPollQuery *APollQuery) {
            clock_t start = clock();

            auto LConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->PollConnection());

            if (LConnection != nullptr) {

                auto LReply = LConnection->Reply();

                CReply::status_type LStatus = CReply::internal_server_error;

                try {
                    QueryToJson(APollQuery, LReply->Content);
                    LStatus = CReply::ok;
                } catch (Delphi::Exception::Exception &E) {
                    ExceptionToJson(0, &E, LReply->Content);
                    Log()->Error(APP_LOG_EMERG, 0, E.what());
                }

                LConnection->SendReply(LStatus, nullptr, true);
            }

            log_debug1(APP_LOG_DEBUG_CORE, Log(), 0, _T("Query executed runtime: %.2f ms."), (double) ((clock() - start) / (double) CLOCKS_PER_SEC * 1000));
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::QueryStart(CHTTPServerConnection *AConnection, const CStringList& SQL) {
            auto LQuery = GetQuery(AConnection);

            if (LQuery == nullptr) {
                Log()->Error(APP_LOG_ALERT, 0, "QueryStart: GetQuery() failed!");
                AConnection->SendStockReply(CReply::internal_server_error);
                return false;
            }

            LQuery->SQL() = SQL;

            if (LQuery->QueryStart() != POLL_QUERY_START_ERROR) {
                // Wait query result...
                AConnection->CloseConnection(false);

                return true;
            } else {
                delete LQuery;
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::APIRun(CHTTPServerConnection *AConnection, const CString &Route, const CString &jsonString,
                const CDataBase &DataBase) {

            CStringList SQL;

            SQL.Add(CString());

            if (Route == "/login") {
                SQL.Last().Format("SELECT * FROM api.run('%s', '%s'::jsonb);",
                                            Route.c_str(), jsonString.IsEmpty() ? "{}" : jsonString.c_str());
            } else if (!DataBase.Session.IsEmpty()) {
                SQL.Last().Format("SELECT * FROM api.run('%s', '%s'::jsonb, '%s');",
                                            Route.c_str(), jsonString.IsEmpty() ? "{}" : jsonString.c_str(), DataBase.Session.c_str());
            } else {
                SQL.Last().Format("SELECT * FROM api.login('%s', '%s');",
                                            DataBase.Username.c_str(), DataBase.Password.c_str());

                SQL.Add(CString());
                SQL.Last().Format("SELECT * FROM api.run('%s', '%s'::jsonb);",
                                            Route.c_str(), jsonString.IsEmpty() ? "{}" : jsonString.c_str());

                SQL.Add("SELECT * FROM api.logout();");
            }

            return QueryStart(AConnection, SQL);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPointDisconnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (LConnection != nullptr) {
                auto LPoint = m_CPManager->FindPointByConnection(LConnection);
                if (LPoint != nullptr) {
                    Log()->Message(_T("[%s:%d] Point %s closed connection."), LConnection->Socket()->Binding()->PeerIP(),
                                   LConnection->Socket()->Binding()->PeerPort(),
                                   LPoint->Identity().IsEmpty() ? "(empty)" : LPoint->Identity().c_str());
                    delete LPoint;
                } else {
                    Log()->Message(_T("[%s:%d] Client closed connection."), LConnection->Socket()->Binding()->PeerIP(),
                                   LConnection->Socket()->Binding()->PeerPort());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            const CString& LSecWebSocketKey = LRequest->Headers.Values("sec-websocket-key");

            if (LSecWebSocketKey.IsEmpty()) {
                AConnection->SendStockReply(CReply::bad_request);
                return;
            }

            AConnection->CloseConnection(false);

            LReply->Status = CReply::switching_protocols;

            LReply->AddHeader("Upgrade", "websocket");
            LReply->AddHeader("Connection", "Upgrade");

            const CString LAccept(SHA1(LSecWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

            LReply->AddHeader("Sec-WebSocket-Accept", base64_encode(LAccept));

            const CString& LSecWebSocketProtocol = LRequest->Headers.Values("sec-websocket-protocol");
            if (!LSecWebSocketProtocol.IsEmpty()) {
                LReply->AddHeader("Sec-WebSocket-Protocol", LSecWebSocketProtocol.SubString(0, LSecWebSocketProtocol.Find(',')));
            }

            AConnection->SendReply();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPost(CHTTPServerConnection *AConnection) {
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CStringList LUri;
            SplitColumns(LRequest->Uri.c_str(), LRequest->Uri.Size(), &LUri, '/');
            if (LUri.Count() < 2) {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            DebugRequest(LRequest);

            auto LPoint = m_CPManager->FindPointByConnection(AConnection);
            if (LPoint == nullptr) {
                LPoint = m_CPManager->Add(AConnection);
                AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
            }

            LPoint->Parse(LRequest->Content, LReply->Content);

            AConnection->SendReply(CReply::ok, "application/soap+xml");

            DebugReply(LReply);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Execute(CHTTPServerConnection *AConnection) {
            int i = 0;
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            LReply->Clear();
            LReply->ContentType = CReply::html;

            CMethodHandler *Handler;
            for (i = 0; i < m_Methods.Count(); ++i) {
                Handler = (CMethodHandler *) m_Methods.Objects(i);
                if (Handler->Allow()) {
                    const CString& Method = m_Methods.Strings(i);
                    if (Method == LRequest->Method) {
                        CORS(AConnection);
                        Handler->Handler(AConnection);
                        break;
                    }
                }
            }

            if (i == m_Methods.Count()) {
                AConnection->SendStockReply(CReply::not_implemented);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::BeforeExecute(Pointer Data) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::AfterExecute(Pointer Data) {

        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckUserAgent(const CString& Value) {
            return true;
        }

    }
}
}