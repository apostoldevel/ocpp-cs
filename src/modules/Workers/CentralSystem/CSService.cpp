/*++

Program name:

  Apostol CSService

Module Name:

  CSService.cpp

Notices:

  Module: Central System Service

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

//----------------------------------------------------------------------------------------------------------------------

#include "Core.hpp"
#include "CSService.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include "jwt.h"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Workers {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCSService::CCSService(CModuleProcess *AProcess) : CApostolModule(AProcess, "ocpp central system service") {
            m_Headers.Add("Authorization");

            CCSService::InitMethods();

            InitOperations();
            InitEndpoints();
        }
        //--------------------------------------------------------------------------------------------------------------
        
        void CCSService::InitMethods() {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            m_Methods.AddObject(_T("GET")    , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoGet(Connection); }));
            m_Methods.AddObject(_T("POST")   , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoPost(Connection); }));
            m_Methods.AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoOptions(Connection); }));
            m_Methods.AddObject(_T("HEAD")   , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoHead(Connection); }));
            m_Methods.AddObject(_T("PUT")    , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_Methods.AddObject(_T("DELETE") , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_Methods.AddObject(_T("TRACE")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_Methods.AddObject(_T("PATCH")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_Methods.AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
#else
            m_Methods.AddObject(_T("GET"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoGet, this, _1)));
            m_Methods.AddObject(_T("POST"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoPost, this, _1)));
            m_Methods.AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoOptions, this, _1)));
            m_Methods.AddObject(_T("HEAD"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoHead, this, _1)));
            m_Methods.AddObject(_T("PUT"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("DELETE"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("TRACE"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("PATCH"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_Methods.AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::InitOperations() {
            
            m_Operations.AddPair("CancelReservation", {});
            m_Operations.Last().Value().Add({"reservationId", "integer", true});

            m_Operations.AddPair("ChangeAvailability", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});
            m_Operations.Last().Value().Add({"type", "AvailabilityType", true});

            m_Operations.AddPair("ChangeConfiguration", {});
            m_Operations.Last().Value().Add({"key", "CiString50Type", true});
            m_Operations.Last().Value().Add({"value", "CiString500Type", true});

            m_Operations.AddPair("ClearCache", {});

            m_Operations.AddPair("ClearChargingProfile", {});
            m_Operations.Last().Value().Add({"id", "integer", false});
            m_Operations.Last().Value().Add({"connectorId", "integer", false});
            m_Operations.Last().Value().Add({"chargingProfilePurpose", "ChargingProfilePurposeType", false});
            m_Operations.Last().Value().Add({"stackLevel", "integer", false});

            m_Operations.AddPair("DataTransfer", {});
            m_Operations.Last().Value().Add({"vendorId", "CiString255Type", true});
            m_Operations.Last().Value().Add({"messageId", "CiString50Type", false});
            m_Operations.Last().Value().Add({"data", "Text", false});

            m_Operations.AddPair("DiagnosticsStatusNotification", {});
            m_Operations.Last().Value().Add({"status", "DiagnosticsStatus", true});

            m_Operations.AddPair("FirmwareStatusNotification", {});
            m_Operations.Last().Value().Add({"vendorId", "FirmwareStatus", true});

            m_Operations.AddPair("GetCompositeSchedule", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});
            m_Operations.Last().Value().Add({"duration", "integer", true});
            m_Operations.Last().Value().Add({"chargingRateUnit", "ChargingRateUnitType", false});

            m_Operations.AddPair("GetConfiguration", {});
            m_Operations.Last().Value().Add({"key", "CiString50Type", false});

            m_Operations.AddPair("GetDiagnostics", {});
            m_Operations.Last().Value().Add({"location", "anyURI", true});
            m_Operations.Last().Value().Add({"retries", "integer", false});
            m_Operations.Last().Value().Add({"retryInterval", "integer", false});
            m_Operations.Last().Value().Add({"startTime", "dateTime", false});
            m_Operations.Last().Value().Add({"stopTime", "dateTime", false});

            m_Operations.AddPair("GetLocalListVersion", {});
            m_Operations.AddPair("Heartbeat", {});

            m_Operations.AddPair("MeterValues", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});
            m_Operations.Last().Value().Add({"transactionId", "integer", true});
            m_Operations.Last().Value().Add({"meterValue", "MeterValue", false});

            m_Operations.AddPair("RemoteStartTransaction", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", false});
            m_Operations.Last().Value().Add({"idTag", "IdToken", true});
            m_Operations.Last().Value().Add({"chargingProfile", "ChargingProfile", false});

            m_Operations.AddPair("RemoteStopTransaction", {});
            m_Operations.Last().Value().Add({"transactionId", "integer", true});

            m_Operations.AddPair("ReserveNow", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});
            m_Operations.Last().Value().Add({"expiryDate", "dateTime", true});
            m_Operations.Last().Value().Add({"idTag", "IdToken", true});
            m_Operations.Last().Value().Add({"parentIdTag", "IdToken", false});
            m_Operations.Last().Value().Add({"reservationId", "integer", true});

            m_Operations.AddPair("Reset", {});
            m_Operations.Last().Value().Add({"type", "ResetType", true});

            m_Operations.AddPair("SendLocalList", {});
            m_Operations.Last().Value().Add({"listVersion", "integer", true});
            m_Operations.Last().Value().Add({"localAuthorizationList", "AuthorizationData", false});
            m_Operations.Last().Value().Add({"updateType", "UpdateType", true});

            m_Operations.AddPair("SetChargingProfile", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});
            m_Operations.Last().Value().Add({"csChargingProfiles", "ChargingProfile", true});

            m_Operations.AddPair("StartTransaction", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});
            m_Operations.Last().Value().Add({"idTag", "IdToken", true});
            m_Operations.Last().Value().Add({"meterStart", "integer", true});
            m_Operations.Last().Value().Add({"reservationId", "integer", false});
            m_Operations.Last().Value().Add({"timestamp", "dateTime", true});

            m_Operations.AddPair("StatusNotification", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});
            m_Operations.Last().Value().Add({"errorCode", "ChargePointErrorCode", true});
            m_Operations.Last().Value().Add({"info", "CiString50Type", false});
            m_Operations.Last().Value().Add({"status", "ChargePointStatus", true});
            m_Operations.Last().Value().Add({"timestamp", "dateTime", false});
            m_Operations.Last().Value().Add({"vendorId", "CiString255Type", false});
            m_Operations.Last().Value().Add({"vendorErrorCode", "CiString50Type", false});

            m_Operations.AddPair("StopTransaction", {});
            m_Operations.Last().Value().Add({"idTag", "IdToken", false});
            m_Operations.Last().Value().Add({"meterStop", "integer", true});
            m_Operations.Last().Value().Add({"timestamp", "dateTime", true});
            m_Operations.Last().Value().Add({"transactionId", "integer", true});
            m_Operations.Last().Value().Add({"reason", "Reason", false});
            m_Operations.Last().Value().Add({"transactionData", "MeterValue", false});

            m_Operations.AddPair("TriggerMessage", {});
            m_Operations.Last().Value().Add({"requestedMessage", "MessageTrigger", true});
            m_Operations.Last().Value().Add({"connectorId", "integer", false});

            m_Operations.AddPair("UnlockConnector", {});
            m_Operations.Last().Value().Add({"connectorId", "integer", true});

            m_Operations.AddPair("UpdateFirmware", {});
            m_Operations.Last().Value().Add({"location", "anyURI", true});
            m_Operations.Last().Value().Add({"retries", "integer", false});
            m_Operations.Last().Value().Add({"retrieveDate", "dateTime", true});
            m_Operations.Last().Value().Add({"retryInterval", "integer", false});
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::InitEndpoints() {
            m_Endpoints.AddPair("ChargePointList", {});

            m_Endpoints.AddPair("TransactionList", {});
            m_Endpoints.Last().Value().Add({"identity", "text", true});
            m_Endpoints.Last().Value().Add({"dateFrom", "timestamp", false});
            m_Endpoints.Last().Value().Add({"dateTo", "timestamp", false});

            m_Endpoints.AddPair("ReservationList", {});
            m_Endpoints.Last().Value().Add({"identity", "text", true});
            m_Endpoints.Last().Value().Add({"dateFrom", "timestamp", false});
            m_Endpoints.Last().Value().Add({"dateTo", "timestamp", false});
        }
        //--------------------------------------------------------------------------------------------------------------

        CHTTPReply::CStatusType CCSService::ErrorCodeToStatus(int ErrorCode) {
            CHTTPReply::CStatusType status = CHTTPReply::ok;

            if (ErrorCode != 0) {
                switch (ErrorCode) {
                    case 401:
                        status = CHTTPReply::unauthorized;
                        break;

                    case 403:
                        status = CHTTPReply::forbidden;
                        break;

                    case 404:
                        status = CHTTPReply::not_found;
                        break;

                    case 500:
                        status = CHTTPReply::internal_server_error;
                        break;

                    default:
                        status = CHTTPReply::bad_request;
                        break;
                }
            }

            return status;
        }
        //--------------------------------------------------------------------------------------------------------------

        int CCSService::CheckError(const CJSON &Json, CString &ErrorMessage, bool RaiseIfError) {
            int errorCode = 0;

            if (Json.HasOwnProperty(_T("error"))) {
                const auto& error = Json[_T("error")];

                if (error.HasOwnProperty(_T("code"))) {
                    errorCode = error[_T("code")].AsInteger();
                } else {
                    return 0;
                }

                if (error.HasOwnProperty(_T("message"))) {
                    ErrorMessage = error[_T("message")].AsString();
                } else {
                    return 0;
                }

                if (RaiseIfError)
                    throw EDBError(ErrorMessage.c_str());

                if (errorCode >= 10000)
                    errorCode = errorCode / 100;

                if (errorCode < 0)
                    errorCode = 400;
            }

            return errorCode;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::ConnectionExists(CWebSocketConnection *AConnection) {
            if (AConnection == nullptr) {
                return false;
            }

            AConnection->Unlock();

            if (AConnection->ClosedGracefully()) {
                if (AConnection->AutoFree() && !AConnection->Locked()) {
                    delete AConnection;
                }

                return false;
            }

            return Server().IndexOfConnection(AConnection) != -1;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SendError(CHTTPServerConnection *AConnection, const CHTTPReply::CStatusType ErrorCode, const CString &Message) {
            auto &Reply = AConnection->Reply();

            Reply.ContentType = CHTTPReply::json;

            Reply.Content.Clear();
            Reply.Content.Format(R"({"error": {"code": %u, "message": "%s"}})", ErrorCode, Delphi::Json::EncodeJsonString(Message).c_str());

            AConnection->SendReply(CHTTPReply::ok, nullptr, true);

            Log()->Error(ErrorCode == CHTTPReply::internal_server_error ? APP_LOG_EMERG : APP_LOG_ERR, 0, _T("Error: %s"), Message.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CJSON CCSService::GetChargePointList() {
            CJSON json;
            CJSONValue jsonArray(jvtArray);

            for (int i = 0; i < m_PointManager.Count(); i++) {
                CJSONValue jsonPoint(jvtObject);
                CJSONValue jsonConnection(jvtObject);

                const auto pPoint = m_PointManager.Points(i);

                jsonPoint.Object().AddPair("Identity", pPoint->Identity());
                jsonPoint.Object().AddPair("Address", pPoint->Address());

                const auto pConnection = pPoint->Connection();

                if (ConnectionExists(pConnection)) {
                    jsonConnection.Object().AddPair("Socket", pConnection->Socket()->Binding()->Handle());
                    jsonConnection.Object().AddPair("IP", pConnection->Socket()->Binding()->PeerIP());
                    jsonConnection.Object().AddPair("Port", pConnection->Socket()->Binding()->PeerPort());

                    jsonPoint.Object().AddPair("Connection", jsonConnection);
                } else {
                    jsonPoint.Object().AddPair("Connection", CJSONValue());
                }

                jsonArray.Array().Add(jsonPoint);
            }

            json.Object().AddPair("ChargePointList", jsonArray);

            return json;
        }
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_POSTGRESQL
        void CCSService::DoPostgresQueryExecuted(CPQPollQuery *APollQuery) {
            try {
                for (int i = 0; i < APollQuery->Count(); i++) {
                    const auto pResult = APollQuery->Results(i);

                    if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                        throw Delphi::Exception::EDBError(pResult->GetErrorMessage());
                }
            } catch (std::exception &e) {
                Log()->Error(APP_LOG_ERR, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
            Log()->Error(APP_LOG_ERR, 0, E.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SetPointConnected(CCSChargingPoint *APoint, bool Value) {

            auto OnExecuted = [](CPQPollQuery *APollQuery) {
                try {
                    for (int i = 0; i < APollQuery->Count(); i++) {
                        const auto pResult = APollQuery->Results(i);

                        if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                            throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                    }
                } catch (Delphi::Exception::Exception &E) {
                    Log()->Error(APP_LOG_ERR, 0, E.what());
                }
            };

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                Log()->Error(APP_LOG_ERR, 0, E.what());
            };

            CStringList SQL;

            SQL.Add(CString().Format("SELECT * FROM ocpp.SetChargePointConnected('%s', %s);", APoint->Identity().c_str(), Value ? "true" : "false"));

            try {
                ExecSQL(SQL, nullptr, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                Log()->Error(APP_LOG_ERR, 0, E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::ParseSOAP(CHTTPServerConnection *AConnection, const CString &Payload) {

            auto OnExecuted = [this](CPQPollQuery *APollQuery) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (ConnectionExists(pConnection)) {
                    auto &Reply = pConnection->Reply();

                    try {
                        for (int i = 0; i < APollQuery->Count(); i++) {
                            const auto pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            const CString caIdentity(pResult->GetValue(0, 0));
                            const CString caAction(pResult->GetValue(0, 1));
                            const CString caAddress(pResult->GetValue(0, 2));
                            const CString caPayload(pResult->GetValue(0, 3));

                            if (caIdentity.IsEmpty())
                                throw Delphi::Exception::EDBError("Identity cannot by empty.");

                            auto pPoint = m_PointManager.FindPointByIdentity(caIdentity);

                            if (pPoint == nullptr) {
                                pPoint = m_PointManager.Add(nullptr);
                                pPoint->ProtocolType(ChargePoint::ptSOAP);
                                pPoint->Identity() = caIdentity;
                            }

                            pPoint->Address() = caAddress;

                            Reply.ContentType = CHTTPReply::xml;
                            Reply.Content = R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED;
                            Reply.Content << caPayload;

                            pPoint->UpdateConnected(true);
                            DoPointConnected(pPoint);

                            pConnection->SendReply(CHTTPReply::ok, "application/soap+xml", true);
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        SOAPError(pConnection, "Receiver", "InternalError", "Parser exception", E.what());
                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }
                }
            };

            auto OnException = [this](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                if (ConnectionExists(pConnection)) {
                    SOAPError(pConnection, "Receiver", "InternalError", "SQL exception.", E.what());
                }
            };

            CStringList SQL;

            SQL.Add(CString()
                .MaxFormatSize(256 + Payload.Size())
                .Format("SELECT * FROM ocpp.ParseXML(%s::xml);", PQQuoteLiteral(Payload).c_str()));

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
                AConnection->Lock();
            } catch (Delphi::Exception::Exception &E) {
                SOAPError(AConnection, "Receiver", "InternalError", "ExecSQL call failed.", E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::ParseJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CJSONMessage &Message, const CString &Account) {

            auto OnExecuted = [this](CPQPollQuery *APollQuery) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (ConnectionExists(pConnection)) {
                    auto &WSReply = pConnection->WSReply();

                    CJSONMessage message;
                    CString identity;
                    CString response;

                    try {
                        CJSON result;
                        for (int i = 0; i < APollQuery->Count(); i++) {
                            const auto pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            result << pResult->GetValue(0, 0);

                            identity = result["identity"].AsString();

                            message.MessageTypeId = COCPPMessage::StringToMessageTypeId(
                                    result["messageTypeId"].AsString());
                            message.UniqueId = result["uniqueId"].AsString();
                            message.Action = result["action"].AsString();

                            if (message.MessageTypeId == ChargePoint::mtCallError) {
                                message.ErrorCode = result["errorCode"].AsString();
                                message.ErrorDescription = result["errorDescription"].AsString();
                                message.Payload << result["errorDescription"];
                            } else {
                                message.Payload << result["payload"].ToString();
                            }
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        message.MessageTypeId = ChargePoint::mtCallError;
                        message.ErrorCode = "InternalError";
                        message.ErrorDescription = E.what();

                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }

                    CJSONProtocol::Response(message, response);

                    WSReply.SetPayload(response);
                    pConnection->SendWebSocket(true);

                    LogJSONMessage(identity, message);
                }
            };

            auto OnException = [this](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                if (ConnectionExists(pConnection)) {
                    DoWebSocketError(pConnection, E);
                }
            };

            LogJSONMessage(Identity, Message);

            auto const &payload = Message.Payload.ToString();

            CStringList SQL;

            SQL.Add(CString()
                .MaxFormatSize(256 + Identity.Size() + Message.Size() + payload.Size() + Account.Size())
                .Format("SELECT * FROM ocpp.Parse(%s, %s, %s, %s::jsonb, %s);",
                        PQQuoteLiteral(Identity).c_str(),
                        PQQuoteLiteral(Message.UniqueId).c_str(),
                        PQQuoteLiteral(Message.Action).c_str(),
                        PQQuoteLiteral(payload).c_str(),
                        PQQuoteLiteral(Account).c_str())
            );

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
                AConnection->Lock();
            } catch (Delphi::Exception::Exception &E) {
                DoWebSocketError(AConnection, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::JSONToSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation,
            const CJSON &Payload) {

            auto OnExecuted = [this, APoint](CPQPollQuery *APollQuery) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (ConnectionExists(pConnection)) {
                    try {
                        const auto &csOperation = pConnection->Data()["operation"];

                        for (int i = 0; i < APollQuery->Count(); i++) {
                            const auto pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            CString caPayload(R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED);
                            caPayload << pResult->GetValue(0, 0);

                            SendSOAP(pConnection, APoint, csOperation, caPayload);
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ReplyError(pConnection, CHTTPReply::bad_request, E.what());
                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }
                }
            };

            auto OnException = [this](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                if (ConnectionExists(pConnection)) {
                    ReplyError(pConnection, CHTTPReply::internal_server_error, E.what());
                }
            };

            CStringList SQL;

            CJSON Data;
            CJSONObject Header;

            Header.AddPair("chargeBoxIdentity", APoint->Identity());
            Header.AddPair("Action", Operation);
            Header.AddPair("To", APoint->Address());

            Data.Object().AddPair("header", Header);
            Data.Object().AddPair("payload", Payload.Object());

            const auto &caData = Data.ToString();

            SQL.Add(CString()
                .MaxFormatSize(256 + caData.Size())
                .Format("SELECT * FROM ocpp.JSONToSOAP(%s::jsonb);", PQQuoteLiteral(caData).c_str())
            );

            AConnection->Data().Values("operation", Operation);

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
                AConnection->Lock();
            } catch (Delphi::Exception::Exception &E) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SOAPToJSON(CHTTPServerConnection *AConnection, const CString &Payload) {

            auto OnExecuted = [this](CPQPollQuery *APollQuery) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (ConnectionExists(pConnection)) {
                    auto &Reply = pConnection->Reply();

                    Reply.ContentType = CHTTPReply::json;

                    try {
                        for (int i = 0; i < APollQuery->Count(); i++) {
                            const auto pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            Reply.Content = pResult->GetValue(0, 0);
                            pConnection->SendReply(CHTTPReply::ok, "application/json", true);

                            DebugReply(Reply);
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ReplyError(pConnection, CHTTPReply::bad_request, E.what());
                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }
                }
            };

            auto OnException = [this](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                if (ConnectionExists(pConnection)) {
                    ReplyError(pConnection, CHTTPReply::internal_server_error, E.what());
                }
            };

            CStringList SQL;

            SQL.Add(CString()
                .MaxFormatSize(256 + Payload.Size())
                .Format("SELECT * FROM ocpp.SOAPToJSON(%s::xml);", PQQuoteLiteral(Payload).c_str())
            );

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
                AConnection->Lock();
            } catch (Delphi::Exception::Exception &E) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, E.what());
            }
        }
#endif
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::LogJSONMessage(const CString &Identity, const CJSONMessage &Message) {
            Log()->Message("[%s] [%s] [%s] [%s] %s",
                Identity.IsEmpty() ? "(empty)" : Identity.c_str(),
                Message.UniqueId.IsEmpty() ? "(empty)" : Message.UniqueId.c_str(),
                Message.Action.IsEmpty() ? "(empty)" : Message.Action.c_str(),
                COCPPMessage::MessageTypeIdToString(Message.MessageTypeId).c_str(),
                Message.Payload.IsNull() ? "{}" : Message.Payload.ToString().c_str()
            );
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::OnChargePointMessageSOAP(CObject *Sender, const CSOAPMessage &Message) {
            const auto pPoint = dynamic_cast<CCSChargingPoint *> (Sender);
            chASSERT(pPoint);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::OnChargePointMessageJSON(CObject *Sender, const CJSONMessage &Message) {
            const auto pPoint = dynamic_cast<CCSChargingPoint *> (Sender);
            if (pPoint != nullptr)
                LogJSONMessage(pPoint->Identity(), Message);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPointConnected(CCSChargingPoint *APoint) {
#ifdef WITH_POSTGRESQL
            if (APoint->UpdateConnected())
                SetPointConnected(APoint, true);
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPointDisconnected(CObject *Sender) {
            const auto pConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (pConnection != nullptr) {
                const auto pSocket = pConnection->Socket();

                try {
                    const auto pPoint = dynamic_cast<CCSChargingPoint *> (pConnection->Object());

                    if (pPoint != nullptr) {
#ifdef WITH_POSTGRESQL
                        if (pPoint->UpdateConnected()) {
                            SetPointConnected(pPoint, false);
                        }
#endif
                        if (pSocket != nullptr) {
                            const auto pHandle = pSocket->Binding();
                            if (pHandle != nullptr) {
                                Log()->Notice(_T("[%s:%d] Point \"%s\" closed connection."),
                                               pHandle->PeerIP(), pHandle->PeerPort(),
                                               pPoint->Identity().IsEmpty() ? "(empty)" : pPoint->Identity().c_str()
                                               );
                            }
                        } else {
                            Log()->Notice(_T("Point \"%s\" closed connection."),
                                           pPoint->Identity().IsEmpty() ? "(empty)" : pPoint->Identity().c_str());
                        }

                        if (pPoint->UpdateCount() == 0) {
                            delete pPoint;
                        }
                    } else {
                        if (pSocket != nullptr) {
                            const auto pHandle = pSocket->Binding();
                            if (pHandle != nullptr) {
                                Log()->Notice(_T("[%s:%d] Unknown point closed connection."), pHandle->PeerIP(), pHandle->PeerPort());
                            }
                        } else {
                            Log()->Warning(_T("Unknown point closed connection."));
                        }
                    }
                } catch (Delphi::Exception::Exception &E) {
                    Log()->Error(APP_LOG_ERR, 0, E.what());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoWebSocketError(CHTTPServerConnection *AConnection, const Delphi::Exception::Exception &E) {

            Log()->Error(APP_LOG_ERR, 0, E.what());

            if (ConnectionExists(AConnection)) {
                const auto &caWSRequest = AConnection->WSRequest();
                auto &WSReply = AConnection->WSReply();

                const CString csRequest(caWSRequest.Payload());

                CJSONMessage jmRequest;
                CJSONProtocol::Request(csRequest, jmRequest);

                CJSONMessage jmResponse;
                CString sResponse;

                CJSON jResult;

                CJSONProtocol::PrepareResponse(jmRequest, jmResponse);

                jmResponse.MessageTypeId = ChargePoint::mtCallError;
                jmResponse.ErrorCode = "InternalError";
                jmResponse.ErrorDescription = E.what();

                CJSONProtocol::Response(jmResponse, sResponse);

                WSReply.SetPayload(sResponse);
                AConnection->SendWebSocket(true);

                auto pPoint = dynamic_cast<CCSChargingPoint *> (AConnection->Object());

                if (pPoint != nullptr) {
                    Log()->Message("[%s] [%s] [%s] [%s] %s", pPoint->Identity().c_str(),
                                   jmResponse.UniqueId.c_str(),
                                   jmResponse.Action.IsEmpty() ? "Unknown" : jmResponse.Action.c_str(),
                                   COCPPMessage::MessageTypeIdToString(jmResponse.MessageTypeId).c_str(),
                                   jmResponse.Payload.IsNull() ? "{}" : jmResponse.Payload.ToString().c_str());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SOAPError(CHTTPServerConnection *AConnection, const CString &Code, const CString &SubCode,
                const CString &Reason, const CString &Message) {

            auto &Reply = AConnection->Reply();

            Reply.ContentType = CHTTPReply::xml;

            Reply.Content.Clear();
            Reply.Content = CSOAPProtocol::Error(Code, SubCode, Reason, Message);

            AConnection->SendReply(CHTTPReply::ok, "application/soap+xml", true);

            Log()->Error(APP_LOG_ERR, 0, _T("SOAPError: %s"), Message.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_AUTHORIZATION
        void CCSService::VerifyToken(const CString &Token) {

            const auto& GetSecret = [](const CProvider &Provider, const CString &Application) {
                const auto &Secret = Provider.Secret(Application);
                if (Secret.IsEmpty())
                    throw ExceptionFrm("Not found secret for \"%s:%s\"", Provider.Name().c_str(), Application.c_str());
                return Secret;
            };

            auto decoded = jwt::decode(Token);
            const auto& aud = CString(decoded.get_audience());

            CString Application;

            const auto& Providers = Server().Providers();

            const auto index = OAuth2::Helper::ProviderByClientId(Providers, aud, Application);
            if (index == -1)
                throw COAuth2Error(_T("Not found provider by Client ID."));

            const auto& Provider = Providers[index].Value();

            const auto& iss = CString(decoded.get_issuer());

            CStringList Issuers;
            Provider.GetIssuers(Application, Issuers);
            if (Issuers[iss].IsEmpty())
                throw jwt::error::token_verification_exception(jwt::error::token_verification_error::issuer_missmatch);

            const auto& alg = decoded.get_algorithm();

            const auto& caSecret = GetSecret(Provider, Application);

            if (alg == "HS256") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{caSecret});
                verifier.verify(decoded);
            } else if (alg == "HS384") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs384{caSecret});
                verifier.verify(decoded);
            } else if (alg == "HS512") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs512{caSecret});
                verifier.verify(decoded);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckAuthorizationData(const CHTTPRequest &Request, CAuthorization &Authorization) {

            const auto &caHeaders = Request.Headers;

            const auto &caAuthorization = caHeaders.Values(_T("Authorization"));

            if (caAuthorization.IsEmpty()) {

                const auto &headerSession = caHeaders.Values(_T("Session"));
                const auto &headerSecret = caHeaders.Values(_T("Secret"));

                Authorization.Username = headerSession;
                Authorization.Password = headerSecret;

                if (Authorization.Username.IsEmpty() || Authorization.Password.IsEmpty())
                    return false;

                Authorization.Schema = CAuthorization::asBasic;
                Authorization.Type = CAuthorization::atSession;

            } else {
                Authorization << caAuthorization;
            }

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization) {

            const auto &caRequest = AConnection->Request();

            try {
                if (CheckAuthorizationData(caRequest, Authorization)) {
                    if (Authorization.Schema == CAuthorization::asBearer) {
                        VerifyToken(Authorization.Token);
                        return true;
                    }
                }

                if (Authorization.Schema == CAuthorization::asBasic)
                    AConnection->Data().Values("Authorization", "Basic");

                ReplyError(AConnection, CHTTPReply::unauthorized, "Unauthorized.");
            } catch (jwt::error::token_expired_exception &e) {
                ReplyError(AConnection, CHTTPReply::forbidden, e.what());
            } catch (jwt::error::token_verification_exception &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            } catch (CAuthorizationError &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------
#endif
        void CCSService::SendJSON(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CJSONMessage &Message) {

            auto OnRequest = [this, AConnection, APoint](COCPPMessageHandler *AHandler, CWebSocketConnection *AWSConnection) {

                auto &WSRequest = AWSConnection->WSRequest();

                if (ConnectionExists(AConnection)) {
                    auto &Reply = AConnection->Reply();
                    const CString Request(WSRequest.Payload());

                    CHTTPReply::CStatusType Status = CHTTPReply::ok;

                    try {
                        CJSONMessage jmMessage;

                        CJSONProtocol::PrepareResponse(AHandler->Message(), jmMessage);

                        if (!CJSONProtocol::Request(Request, jmMessage)) {
                            Status = CHTTPReply::bad_request;
                        }

                        if (APoint != nullptr) {
                            LogJSONMessage(APoint->Identity(), jmMessage);
                        }

                        if (jmMessage.MessageTypeId == ChargePoint::mtCallError) {
                            ReplyError(AConnection, CHTTPReply::bad_request,
                                       CString().Format("%s: %s",
                                                        jmMessage.ErrorCode.c_str(),
                                                        jmMessage.ErrorDescription.c_str()
                                       ));
                        } else {
                            Reply.Content = jmMessage.Payload.ToString();
                            AConnection->SendReply(Status, "application/json", true);
                        }
                    } catch (std::exception &e) {
                        ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
                    }
                }

                AWSConnection->ConnectionStatus(csReplySent);
                WSRequest.Clear();
            };

            AConnection->Lock();
            APoint->Messages().Send(Message, OnRequest);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SendSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation,
                const CString &Payload) {

            auto OnRequest = [](CHTTPClient *Sender, CHTTPRequest &Request) {

                const auto &uri = Sender->Data()["uri"];
                const auto &payload = Sender->Data()["payload"];

                Request.Content = payload;

                CHTTPRequest::Prepare(Request, "POST", uri.c_str(), "application/soap+xml");

                DebugRequest(Request);
            };

            auto OnExecute = [this, AConnection](CTCPConnection *AClientConnection) {

                const auto pConnection = dynamic_cast<CHTTPClientConnection *> (AClientConnection);

                if (ConnectionExists(AConnection) && pConnection != nullptr) {
                    const auto &caClientReply = pConnection->Reply();

                    DebugReply(caClientReply);

                    auto &Reply = AConnection->Reply();

                    Reply.ContentType = CHTTPReply::json;

                    if (Reply.Status == CHTTPReply::ok) {
                        CJSON Json;
                        CSOAPProtocol::SOAPToJSON(caClientReply.Content, Json);

                        Reply.Content = Json.ToString();
                        AConnection->SendReply(Reply.Content.IsEmpty() ? CHTTPReply::no_content : CHTTPReply::ok,
                                               "application/json", true);
                    } else {
                        SendError(AConnection, Reply.Status, Reply.StatusText);
                    }

                    DebugReply(Reply);

                    pConnection->CloseConnection(true);
                }

                return true;
            };
#ifdef WITH_POSTGRESQL
            auto OnExecutePostgres = [this, AConnection](CTCPConnection *AClientConnection) {

                const auto pConnection = dynamic_cast<CHTTPClientConnection *> (AClientConnection);

                if (ConnectionExists(AConnection) && pConnection != nullptr) {
                    auto &Reply = pConnection->Reply();

                    DebugReply(Reply);

                    Reply.ContentType = CHTTPReply::json;

                    if (Reply.Status == CHTTPReply::ok) {
                        SOAPToJSON(AConnection, Reply.Content);
                    } else {
                        SendError(AConnection, Reply.Status, Reply.StatusText);
                    }

                    pConnection->CloseConnection(true);
                }

                return true;
            };
#endif
            auto OnException = [this, AConnection](CTCPConnection *AClientConnection, const Delphi::Exception::Exception &E) {
                const auto pConnection = dynamic_cast<CHTTPClientConnection *> (AClientConnection);
                if (ConnectionExists(AConnection) && pConnection != nullptr) {
                    const auto pClient = dynamic_cast<CHTTPClient *> (pConnection->Client());
                    Log()->Error(APP_LOG_ERR, 0, "[%s:%d] %s", pClient->Host().IsEmpty() ? "empty" : pClient->Host().c_str(), pClient->Port(), E.what());
                }
            };

            AConnection->Lock();

            const CLocation uri(APoint->Address());

            auto pClient = GetClient(uri.hostname, uri.port);

            pClient->Data().Values("uri", uri.href());
            pClient->Data().Values("payload", Payload);

            pClient->OnRequest(OnRequest);
#ifdef WITH_POSTGRESQL
            pClient->OnExecute(OnExecutePostgres);
#else
            pClient->OnExecute(OnExecute);
#endif
            pClient->OnException(OnException);

            pClient->Active(true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::WebhookJSON(CHTTPServerConnection *AConnection, const CString &Identity,
            const CJSONMessage &Message, const CString &Account) {

            auto OnRequest = [this](CHTTPClient *Sender, CHTTPRequest &Request) {
                Request.Content = Sender->Data()["content"];

                CHTTPRequest::Prepare(Request, "POST", m_Webhook.URL().href().c_str(), "application/json");

                if (m_Webhook.Authorization().Schema == CAuthorization::asBasic) {
                    CHTTPRequest::Authorization(Request, "Basic", m_Webhook.Authorization().Username.c_str(), m_Webhook.Authorization().Password.c_str());
                } else if (m_Webhook.Authorization().Schema == CAuthorization::asBearer) {
                    Request.AddHeader("Authorization", CString().Format("Bearer %s", m_Webhook.Authorization().Token).c_str());
                }

                DebugRequest(Request);
            };

            auto OnExecute = [this, AConnection](CTCPConnection *AProxyConnection) {

                const auto pProxyConnection = dynamic_cast<CHTTPClientConnection *> (AProxyConnection);

                if (ConnectionExists(AConnection) && pProxyConnection != nullptr) {
                    const auto &caProxyReply = pProxyConnection->Reply();

                    DebugReply(caProxyReply);

                    CJSONMessage message;
                    CString identity;
                    CString response;

                    if (caProxyReply.Status == CHTTPReply::ok) {
                        const CJSON jContent(caProxyReply.Content);

                        identity = jContent["identity"].AsString();

                        message.MessageTypeId = COCPPMessage::StringToMessageTypeId(jContent["messageTypeId"].AsString());
                        message.UniqueId = jContent["uniqueId"].AsString();
                        message.Action = jContent["action"].AsString();

                        if (message.MessageTypeId == ChargePoint::mtCallError) {
                            message.ErrorCode = jContent["errorCode"].AsString();
                            message.ErrorDescription = jContent["errorDescription"].AsString();
                            message.Payload << jContent["errorDescription"];
                        } else {
                            message.Payload << jContent["payload"].ToString();
                        }
                    } else {
                        const auto pClient = dynamic_cast<CHTTPClient *> (pProxyConnection->Client());
                        if (pClient != nullptr) {
                            identity = pClient->Data()["identity"];
                            message.UniqueId = pClient->Data()["uniqueId"];
                            message.Action = pClient->Data()["action"];
                        }

                        message.MessageTypeId = ChargePoint::mtCallError;
                        message.ErrorCode = "InternalError";
                        message.ErrorDescription = caProxyReply.StatusText;
                    }

                    auto &WSReply = AConnection->WSReply();

                    CJSONProtocol::Response(message, response);

                    WSReply.SetPayload(response);
                    AConnection->SendWebSocket(true);

                    LogJSONMessage(identity, message);
                }

                return true;
            };

            auto OnException = [this, AConnection](CTCPConnection *AProxyConnection, const Delphi::Exception::Exception &E) {
                const auto pConnection = dynamic_cast<CHTTPClientConnection *> (AProxyConnection);
                if (ConnectionExists(AConnection) && pConnection != nullptr) {
                    DoWebSocketError(AConnection, E);
                    const auto pClient = dynamic_cast<CHTTPClient *> (pConnection->Client());
                    if (pClient != nullptr) {
                        Log()->Error(APP_LOG_ERR, 0, "[%s:%d] %s", pClient->Host().IsEmpty() ? "empty" : pClient->Host().c_str(), pClient->Port(), E.what());
                    }
                }
            };

            CJSON jContent;

            jContent.Object().AddPair("identity", Identity);
            jContent.Object().AddPair("uniqueId", Message.UniqueId);
            jContent.Object().AddPair("action", Message.Action);
            jContent.Object().AddPair("payload", Message.Payload.Object());
            jContent.Object().AddPair("account", Account);

            AConnection->Lock();

            const auto pClient = GetClient(m_Webhook.URL().hostname, m_Webhook.URL().port);

            pClient->Data().Values("identity", Identity);
            pClient->Data().Values("uniqueId", Message.UniqueId);
            pClient->Data().Values("action", Message.Action);
            pClient->Data().Values("content", jContent.ToString());

            pClient->OnRequest(OnRequest);
            pClient->OnExecute(OnExecute);
            pClient->OnException(OnException);

            pClient->Active(true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::WebhookSOAP(CHTTPServerConnection *AConnection, const CString &Payload) {
            AConnection->SendStockReply(CHTTPReply::not_implemented);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {

            const auto &caRequest = AConnection->Request();

            const CString sPath(caRequest.Location.pathname);

            // Request path must be absolute and not contain "..".
            if (sPath.empty() || sPath.front() != '/' || sPath.find(_T("..")) != CString::npos) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            if (sPath.SubString(0, 6) == "/ocpp/") {
                if (DoOCPP(AConnection) >= 0)
                    return;
            }

            if (sPath.SubString(0, 5) == "/api/") {
                DoAPI(AConnection);
                return;
            }

            CStringList TryFiles;

            TryFiles.Add("/index.html");

            SendResource(AConnection, sPath, nullptr, false, TryFiles);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPost(CHTTPServerConnection *AConnection) {

            const auto &caRequest = AConnection->Request();

            CStringList slPath;
            SplitColumns(caRequest.Location.pathname, slPath, '/');

            if (slPath.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& caService = slPath[0];

            if (caService == "api") {
                DoAPI(AConnection);
                return;
            }

            if (caService == "Ocpp") {
                DoSOAP(AConnection);
                return;
            }

            AConnection->SendStockReply(CHTTPReply::not_found);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoCentralSystem(CHTTPServerConnection *AConnection, const CString &Token, const CString &Endpoint) {

            const auto &caRequest = AConnection->Request();

            const auto index = m_Endpoints.IndexOfName(Endpoint);
            if (index == -1) {
                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid endpoint: %s", Endpoint.c_str()));
                return;
            }

            auto IndexOfName = [](const CFields &Fields, const CString &Name) {
                for (int i = 0; i < Fields.Count(); ++i) {
                    const auto &field = Fields[i];
                    if (field.name == Name)
                        return i;
                }
                return -1;
            };

            CJSON Payload;
            ContentToJson(caRequest, Payload);

            const auto &caFields = m_Endpoints[index].Value();
            auto &Object = Payload.Object();

            for (int i = 0; i < Object.Count(); i++) {
                const auto& caMember = Object.Members(i);
                const auto& caKey = caMember.String();

                if (IndexOfName(caFields, caKey) == -1) {
                    ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid key: %s", caKey.c_str()));
                    return;
                }

                const auto& caValue = caMember.Value();
                if (caValue.IsNull() || (caValue.ValueType() >= Delphi::Json::jvtString && caValue.IsEmpty())) {
                    Object.Delete(i);
                }
            }

            for (int i = 0; i < caFields.Count(); i++) {
                const auto &field = caFields[i];
                if (field.required && !Payload.HasOwnProperty(field.name)) {
                    ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found required key: %s (%s)", field.name.c_str(), field.type.c_str()));
                    return;
                }
            }
#ifdef WITH_POSTGRESQL
            auto OnExecuted = [this](CPQPollQuery *APollQuery) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (ConnectionExists(pConnection)) {
                    const auto &endpoint = pConnection->Data()["endpoint"];

                    try {
                        CString errorMessage;
                        auto &Reply = pConnection->Reply();
                        CHTTPReply::CStatusType status = CHTTPReply::ok;

                        for (int i = 0; i < APollQuery->Count(); i++) {
                            const auto pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            if (pResult->nTuples() == 1) {
                                const CJSON Value(pResult->GetValue(0, 0));
                                status = ErrorCodeToStatus(CheckError(Value, errorMessage));
                            }

                            PQResultToJson(pResult, Reply.Content, "object", endpoint);

                            if (status == CHTTPReply::ok) {
                                pConnection->SendReply(status, nullptr, true);
                            } else {
                                ReplyError(pConnection, status, errorMessage);
                            }
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ReplyError(pConnection, CHTTPReply::bad_request, E.what());
                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }
                }
            };

            auto OnException = [this](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                const auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                if (ConnectionExists(pConnection)) {
                    ReplyError(pConnection, CHTTPReply::internal_server_error, E.what());
                }
            };

            AConnection->Data().Values("endpoint", Endpoint);

            CStringList SQL;

            const auto &caPayload = Endpoint == "ChargePointList" ? GetChargePointList().ToString() : Payload.ToString();

            SQL.Add(CString()
                            .MaxFormatSize(256 + Token.Size() + caPayload.Size())
                            .Format("SELECT * FROM ocpp.%s(%s, %s::jsonb);",
                                    Endpoint.c_str(),
                                    PQQuoteLiteral(Token).c_str(),
                                    PQQuoteLiteral(caPayload).c_str()
                            )
            );

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
                AConnection->Lock();
            } catch (Delphi::Exception::Exception &E) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, E.what());
            }
#else
            AConnection->SendStockReply(CHTTPReply::not_implemented);
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoChargePoint(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Operation) {

            const auto &caRequest = AConnection->Request();

            const auto index = m_Operations.IndexOfName(Operation);
            if (index == -1) {
                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid operation: %s", Operation.c_str()));
                return;
            }

            const auto pPoint = m_PointManager.FindPointByIdentity(Identity);

            if (pPoint == nullptr) {
                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found Charge Point by Identity: %s", Identity.c_str()));
                return;
            }

            auto IndexOfName = [](const CFields &Fields, const CString &Name) {
                for (int i = 0; i < Fields.Count(); ++i) {
                    const auto &field = Fields[i];
                    if (field.name == Name)
                        return i;
                }
                return -1;
            };

            CJSON Payload;
            ContentToJson(caRequest, Payload);

            const auto &caFields = m_Operations[index].Value();
            auto &Object = Payload.Object();

            for (int i = 0; i < Object.Count(); i++) {
                const auto& caMember = Object.Members(i);
                const auto& caKey = caMember.String();

                if (IndexOfName(caFields, caKey) == -1) {
                    ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid key: %s", caKey.c_str()));
                    return;
                }

                const auto& caValue = caMember.Value();
                if (caValue.IsNull() || (caValue.ValueType() >= Delphi::Json::jvtString && caValue.IsEmpty())) {
                    Object.Delete(i);
                }
            }

            for (int i = 0; i < caFields.Count(); i++) {
                const auto &field = caFields[i];
                if (field.required && !Payload.HasOwnProperty(field.name)) {
                    ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found required key: %s (%s)", field.name.c_str(), field.type.c_str()));
                    return;
                }
            }

            const auto pConnection = pPoint->Connection();

            if (ConnectionExists(pConnection) && pConnection->Protocol() == pWebSocket) {

                if (!pConnection->Connected()) {
                    ReplyError(AConnection, CHTTPReply::bad_request, "Charge Point not connected.");
                    return;
                }

                CJSONMessage jmMessage;

                jmMessage.MessageTypeId = ChargePoint::mtCall;
                jmMessage.UniqueId = GenUniqueId();
                jmMessage.Action = Operation;
                jmMessage.Payload = Payload;

                LogJSONMessage(pPoint->Identity(), jmMessage);

                SendJSON(AConnection, pPoint, jmMessage);
            } else {
#ifdef WITH_POSTGRESQL
                JSONToSOAP(AConnection, pPoint, Operation, Payload);
#else
                AConnection->SendStockReply(CHTTPReply::not_implemented);
#endif
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoChargePointList(CHTTPServerConnection *AConnection) {
            auto &Reply = AConnection->Reply();
            Reply.Content = GetChargePointList().ToString();
            AConnection->SendReply(CHTTPReply::ok);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoAPI(CHTTPServerConnection *AConnection) {

            const auto &caRequest = AConnection->Request();
            auto &Reply = AConnection->Reply();

            CStringList slPath;
            SplitColumns(caRequest.Location.pathname, slPath, '/');

            if (slPath.Count() == 0) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            if (slPath.Count() < 3) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto &caVersion = slPath[1];
            const auto &caCommand = slPath[2];

            try {
                if (caVersion == "v1") {
                    Reply.ContentType = CHTTPReply::json;

                    if (caCommand == "ping") {
                        AConnection->SendStockReply(CHTTPReply::ok);
                        return;
                    }

                    if (caCommand == "time") {
                        Reply.Content << "{\"serverTime\": " << CString::ToString(MsEpoch()) << "}";
                        AConnection->SendReply(CHTTPReply::ok);
                        return;
                    }
#ifdef WITH_AUTHORIZATION
                    CAuthorization Authorization;

                    if (!CheckAuthorization(AConnection, Authorization)) {
                        return;
                    }

                    if (caCommand == "ChargePointList") {
                        DoCentralSystem(AConnection, Authorization.Token, caCommand);
                        return;
                    }

                    if (caCommand == "CentralSystem" && slPath.Count() == 4) {
                        DoCentralSystem(AConnection, Authorization.Token, slPath[3]);
                        return;
                    }
#else
                    const auto &caAction = slPath.Count() >= 4 ? slPath[3] : CString();
                    if (caCommand == "ChargePointList" || (caCommand == "CentralSystem" && caAction == "ChargePointList")) {
                        DoChargePointList(AConnection);
                        return;
                    }
#endif
                    if (caCommand == "ChargePoint" && slPath.Count() == 5) {
                        DoChargePoint(AConnection, slPath[3], slPath[4]);
                        return;
                    }
                }

                AConnection->SendStockReply(CHTTPReply::not_found);
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoSOAP(CHTTPServerConnection *AConnection) {

            const auto &caRequest = AConnection->Request();

#ifdef WITH_POSTGRESQL
            // Process the request in the PostgreSQL
            ParseSOAP(AConnection, caRequest.Content);
#else
            auto &Reply = AConnection->Reply();
            auto pPoint = m_PointManager.FindPointByConnection(AConnection);

            if (pPoint == nullptr) {
                pPoint = m_PointManager.Add(AConnection);
                pPoint->ProtocolType(ChargePoint::ptSOAP);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                AConnection->OnDisconnected([this](auto &&Sender) { DoPointDisconnected(Sender); });
#else
                AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
#endif
            }

            pPoint->Parse(caRequest.Content, Reply.Content);

            AConnection->SendReply(CHTTPReply::ok, "application/soap+xml");
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoWebSocket(CHTTPServerConnection *AConnection) {
            try {
                auto pPoint = dynamic_cast<CCSChargingPoint *> (AConnection->Object());
                auto &WSRequest = AConnection->WSRequest();

                const CString Request(WSRequest.Payload());

                while (Request.Position() < Request.Size()) {
                    CJSONMessage Message;
                    const size_t size = CJSONProtocol::Request(Request, Message);

                    if (size == 0)
                        throw ExceptionFrm("Invalid request data: %s", Request.c_str());

                    Request.Position(static_cast<ssize_t>(size));

                    if (Message.MessageTypeId == ChargePoint::mtCall) {
#ifdef WITH_POSTGRESQL
                        // Process the request in the PostgreSQL
                        ParseJSON(AConnection, pPoint->Identity(), Message, pPoint->Account());
#else
                        if (m_Webhook.Enabled()) {
                            WebhookJSON(AConnection, pPoint->Identity(), Message, pPoint->Account());
                        } else {
                            auto &WSReply = AConnection->WSReply();

                            CString Response;

                            pPoint->Parse(Message, Response);

                            WSReply.SetPayload(Response);
                            AConnection->SendWebSocket(true);
                        }
#endif
                    } else {
                        // We will send the response from the charging station to the handler
                        const auto pHandler = pPoint->Messages().FindMessageById(Message.UniqueId);

                        if (Assigned(pHandler)) {
                            pHandler->Handler(AConnection);
                            delete pHandler;
                        }

                        WSRequest.Clear();
                    }
                }
            } catch (std::exception &e) {
                AConnection->Clear();
                Log()->Error(APP_LOG_ERR, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        int CCSService::DoOCPP(CHTTPServerConnection *AConnection) {

            const auto &caRequest = AConnection->Request();
            auto &Reply = AConnection->Reply();

            Reply.ContentType = CHTTPReply::html;

            CStringList slPath;
            SplitColumns(caRequest.Location.pathname, slPath, '/');

            if (slPath.Count() < 2) {
                return -1;
            }

            const auto& caSecWebSocketKey = caRequest.Headers.Values("sec-websocket-key");
            if (caSecWebSocketKey.IsEmpty()) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return 0;
            }

            const auto& caIdentity = slPath.Last();
            if (caIdentity.IsEmpty()) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return 0;
            }

            const CString csAccept(SHA1(caSecWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            const auto& caSecWebSocketProtocol = caRequest.Headers.Values("sec-websocket-protocol");
            const CString csProtocol(caSecWebSocketProtocol.IsEmpty() ? "" : caSecWebSocketProtocol.SubString(0, caSecWebSocketProtocol.Find(',')));

            AConnection->SwitchingProtocols(csAccept, csProtocol);

            auto pPoint = m_PointManager.FindPointByIdentity(caIdentity);

            if (pPoint == nullptr) {
                pPoint = m_PointManager.Add(AConnection);
                pPoint->ProtocolType(ChargePoint::ptJSON);
                pPoint->Identity() = caIdentity;

                if (slPath.Count() == 3) {
                    const auto& caAccount = slPath[1];
                    if (caAccount.Length() == 40) {
                        pPoint->Account() = caAccount;
                    }
                }
            } else {
                pPoint->UpdateConnected(false);
                pPoint->SwitchConnection(AConnection);
            }

            pPoint->Address() = GetHost(AConnection);

            pPoint->UpdateConnected(true);
            DoPointConnected(pPoint);
#ifdef _DEBUG
            WSDebugConnection(AConnection);
#endif
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            pPoint->OnMessageJSON([this](auto && Sender, auto && Message) { OnChargePointMessageJSON(Sender, Message); });
            AConnection->OnDisconnected([this](auto && Sender) { DoPointDisconnected(Sender); });
#else
            pPoint->OnMessageJSON(std::bind(&CCSService::OnChargePointMessageJSON, this, _1, _2));
            AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
#endif
            return 1;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Initialization(CModuleProcess *AProcess) {
            CApostolModule::Initialization(AProcess);

            const auto webhook_url = getenv("WEBHOOK_URL");

            if (webhook_url) {
                m_Webhook.Enabled(true);
                m_Webhook.URL() = webhook_url;
            } else {
                m_Webhook.Enabled(Config()->IniFile().ReadBool("webhook", "enable", false));
                m_Webhook.URL() = Config()->IniFile().ReadString("webhook", "url", "http://localhost:8080/api/v1/ocpp");
            }

            if (m_Webhook.Enabled()) {
                const auto &caAuthorization = Config()->IniFile().ReadString("webhook", "authorization", "");

                if (caAuthorization.Lower() == "basic") {
                    m_Webhook.Authorization().Schema = CAuthorization::asBasic;
                    m_Webhook.Authorization().Username = Config()->IniFile().ReadString("webhook", "username", "");;
                    m_Webhook.Authorization().Password = Config()->IniFile().ReadString("webhook", "password", "");;
                } else if (caAuthorization.Lower() == "bearer") {
                    m_Webhook.Authorization().Schema = CAuthorization::asBearer;
                    m_Webhook.Authorization().Token = Config()->IniFile().ReadString("webhook", "token", "");;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::Execute(CHTTPServerConnection *AConnection) {
            if (AConnection->Protocol() == pWebSocket) {
                DoWebSocket(AConnection);
                return true;
            }
            return CApostolModule::Execute(AConnection);
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::Enabled() {
            if (m_ModuleStatus == msUnknown)
                m_ModuleStatus = msEnabled;
            return m_ModuleStatus == msEnabled;
        }
        //--------------------------------------------------------------------------------------------------------------

    }
}
}