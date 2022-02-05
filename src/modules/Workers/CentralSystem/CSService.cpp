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
        }
        //--------------------------------------------------------------------------------------------------------------
        
        void CCSService::InitMethods() {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            m_pMethods->AddObject(_T("GET")    , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoGet(Connection); }));
            m_pMethods->AddObject(_T("POST")   , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoPost(Connection); }));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoOptions(Connection); }));
            m_pMethods->AddObject(_T("HEAD")   , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoHead(Connection); }));
            m_pMethods->AddObject(_T("PUT")    , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("DELETE") , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("TRACE")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("PATCH")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
#else
            m_pMethods->AddObject(_T("GET"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoGet, this, _1)));
            m_pMethods->AddObject(_T("POST"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoPost, this, _1)));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoOptions, this, _1)));
            m_pMethods->AddObject(_T("HEAD"), (CObject *) new CMethodHandler(true, std::bind(&CCSService::DoHead, this, _1)));
            m_pMethods->AddObject(_T("PUT"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("DELETE"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("TRACE"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PATCH"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CCSService::MethodNotAllowed, this, _1)));
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

        CJSON CCSService::GetChargePointList() {
            CJSON json;
            CJSONValue jsonArray(jvtArray);

            for (int i = 0; i < m_PointManager.Count(); i++) {
                CJSONValue jsonPoint(jvtObject);
                CJSONValue jsonConnection(jvtObject);

                auto pPoint = m_PointManager.Points(i);

                jsonPoint.Object().AddPair("Identity", pPoint->Identity());
                jsonPoint.Object().AddPair("Address", pPoint->Address());

                auto pConnection = pPoint->Connection();

                if (pConnection != nullptr && pConnection->Connected()) {
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

            CPQResult *Result;

            try {
                for (int I = 0; I < APollQuery->Count(); I++) {
                    Result = APollQuery->Results(I);

                    if (Result->ExecStatus() != PGRES_TUPLES_OK)
                        throw Delphi::Exception::EDBError(Result->GetErrorMessage());
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

                CPQResult *Result;

                try {
                    for (int I = 0; I < APollQuery->Count(); I++) {
                        Result = APollQuery->Results(I);

                        if (Result->ExecStatus() != PGRES_TUPLES_OK)
                            throw Delphi::Exception::EDBError(Result->GetErrorMessage());

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

                CPQResult *pResult;

                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (pConnection != nullptr && !pConnection->ClosedGracefully()) {

                    auto pRequest = pConnection->Request();
                    auto pReply = pConnection->Reply();

                    try {
                        for (int i = 0; i < APollQuery->Count(); i++) {
                            pResult = APollQuery->Results(i);

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

                            pReply->ContentType = CHTTPReply::xml;
                            pReply->Content = R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED;
                            pReply->Content << caPayload;

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

            auto OnException = [AConnection](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                SOAPError(AConnection, "Receiver", "InternalError", "SQL exception.", E.what());
            };

            CStringList SQL;

            SQL.Add(CString()
                .MaxFormatSize(256 + Payload.Size())
                .Format("SELECT * FROM ocpp.ParseXML(%s::xml);", PQQuoteLiteral(Payload).c_str()));

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                SOAPError(AConnection, "Receiver", "InternalError", "ExecSQL call failed.", E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::ParseJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CJSONMessage &Message) {

            auto OnExecuted = [](CPQPollQuery *APollQuery) {

                CPQResult *pResult;

                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (pConnection != nullptr && !pConnection->ClosedGracefully()) {

                    auto pWSReply = pConnection->WSReply();

                    CJSONMessage message;
                    CString identity;
                    CString response;
                    CJSON result;

                    try {
                        for (int i = 0; i < APollQuery->Count(); i++) {
                            pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            result << pResult->GetValue(0, 0);

                            identity = result["identity"].AsString();

                            message.MessageTypeId = COCPPMessage::StringToMessageTypeId(result["messageTypeId"].AsString());
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

                    pWSReply->SetPayload(response);
                    pConnection->SendWebSocket(true);

                    LogJSONMessage(identity, message);
                }
            };

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                DoWebSocketError(pConnection, E);
            };

            LogJSONMessage(Identity, Message);

            auto const &payload = Message.Payload.ToString();

            CStringList SQL;

            SQL.Add(CString()
                .MaxFormatSize(256 + Identity.Size() + Message.Size() + payload.Size())
                .Format("SELECT * FROM ocpp.Parse(%s, %s, %s, %s::jsonb);",
                        PQQuoteLiteral(Identity).c_str(),
                        PQQuoteLiteral(Message.UniqueId).c_str(),
                        PQQuoteLiteral(Message.Action).c_str(),
                        PQQuoteLiteral(payload).c_str())
            );

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                DoWebSocketError(AConnection, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::JSONToSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation,
            const CJSON &Payload) {

            auto OnExecuted = [this, APoint](CPQPollQuery *APollQuery) {

                CPQResult *pResult;

                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (pConnection != nullptr && !pConnection->ClosedGracefully()) {
                    try {
                        const auto &csOperation = pConnection->Data()["operation"];

                        for (int i = 0; i < APollQuery->Count(); i++) {
                            pResult = APollQuery->Results(i);

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

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                ReplyError(pConnection, CHTTPReply::internal_server_error, E.what());
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
            } catch (Delphi::Exception::Exception &E) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SOAPToJSON(CHTTPServerConnection *AConnection, const CString &Payload) {

            auto OnExecuted = [](CPQPollQuery *APollQuery) {

                CPQResult *pResult;

                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (pConnection != nullptr && !pConnection->ClosedGracefully()) {

                    auto pRequest = pConnection->Request();
                    auto pReply = pConnection->Reply();

                    pReply->ContentType = CHTTPReply::json;

                    try {
                        for (int i = 0; i < APollQuery->Count(); i++) {
                            pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            pReply->Content = pResult->GetValue(0, 0);
                            pConnection->SendReply(CHTTPReply::ok, "application/json", true);

                            DebugReply(pReply);
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ReplyError(pConnection, CHTTPReply::bad_request, E.what());
                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }
                }
            };

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                ReplyError(pConnection, CHTTPReply::internal_server_error, E.what());
            };

            CStringList SQL;

            SQL.Add(CString()
                .MaxFormatSize(256 + Payload.Size())
                .Format("SELECT * FROM ocpp.SOAPToJSON(%s::xml);", PQQuoteLiteral(Payload).c_str())
            );

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
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
            auto pPoint = dynamic_cast<CCSChargingPoint *> (Sender);
            chASSERT(pPoint);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::OnChargePointMessageJSON(CObject *Sender, const CJSONMessage &Message) {
            auto pPoint = dynamic_cast<CCSChargingPoint *> (Sender);
            chASSERT(pPoint);
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
            auto pConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (pConnection != nullptr) {

                auto pSocket = pConnection->Socket();

                try {
                    auto pPoint = dynamic_cast<CCSChargingPoint *> (CCSChargingPoint::FindOfConnection(pConnection));

                    if (pPoint != nullptr) {
#ifdef WITH_POSTGRESQL
                        if (pPoint->UpdateConnected()) {
                            SetPointConnected(pPoint, false);
                        }
#endif
                        if (pSocket != nullptr) {
                            auto pHandle = pSocket->Binding();
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
                            auto pHandle = pSocket->Binding();
                            if (pHandle != nullptr) {
                                Log()->Notice(_T("[%s:%d] Unknown point closed connection."),
                                               pHandle->PeerIP(), pHandle->PeerPort()
                                               );
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

            if (AConnection != nullptr && !AConnection->ClosedGracefully()) {
                auto pWSRequest = AConnection->WSRequest();
                auto pWSReply = AConnection->WSReply();

                const CString csRequest(pWSRequest->Payload());

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

                pWSReply->SetPayload(sResponse);
                AConnection->SendWebSocket(true);

                auto pPoint = dynamic_cast<CCSChargingPoint *> (CCSChargingPoint::FindOfConnection(AConnection));

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

            auto pReply = AConnection->Reply();

            pReply->ContentType = CHTTPReply::xml;

            pReply->Content.Clear();
            pReply->Content.Format(R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED R"(<se:Envelope xmlns:se="http://www.w3.org/2003/05/soap-envelope" xmlns:wsa5="http://www.w3.org/2005/08/addressing" xmlns:cp="urn://Ocpp/Cp/2015/10/" xmlns:cs="urn://Ocpp/Cs/2015/10/"><se:Body><se:Fault><se:Code><se:Value>se:%s</se:Value><cs:SubCode><se:Value>cs:%s</se:Value></cs:SubCode></se:Code><se:Reason><se:Text>%s<se:/Text></se:Reason><se:Detail><se:Text>%s</se:Text></se:Detail></se:Fault></se:Body></se:Envelope>)", Code.c_str(), SubCode.c_str(), Reason.c_str(), Delphi::Json::EncodeJsonString(Message).c_str());

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
                throw jwt::token_verification_exception("Token doesn't contain the required issuer.");

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

        bool CCSService::CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization) {

            const auto &caHeaders = ARequest->Headers;
            const auto &caCookies = ARequest->Cookies;

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

            auto pRequest = AConnection->Request();

            try {
                if (CheckAuthorizationData(pRequest, Authorization)) {
                    if (Authorization.Schema == CAuthorization::asBearer) {
                        VerifyToken(Authorization.Token);
                        return true;
                    }
                }

                if (Authorization.Schema == CAuthorization::asBasic)
                    AConnection->Data().Values("Authorization", "Basic");

                ReplyError(AConnection, CHTTPReply::unauthorized, "Unauthorized.");
            } catch (jwt::token_expired_exception &e) {
                ReplyError(AConnection, CHTTPReply::forbidden, e.what());
            } catch (jwt::token_verification_exception &e) {
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

            auto OnRequest = [AConnection, APoint](COCPPMessageHandler *AHandler, CWebSocketConnection *AWSConnection) {

                auto pWSRequest = AWSConnection->WSRequest();

                if (AConnection != nullptr && !AConnection->ClosedGracefully()) {

                    auto pReply = AConnection->Reply();
                    const CString pRequest(pWSRequest->Payload());

                    CHTTPReply::CStatusType Status = CHTTPReply::ok;

                    try {
                        CJSONMessage jmMessage;

                        CJSONProtocol::PrepareResponse(AHandler->Message(), jmMessage);

                        if (!CJSONProtocol::Request(pRequest, jmMessage)) {
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
                            pReply->Content = jmMessage.Payload.ToString();
                            AConnection->SendReply(Status, "application/json", true);
                        }
                    } catch (std::exception &e) {
                        ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
                    }
                }

                AWSConnection->ConnectionStatus(csReplySent);
                pWSRequest->Clear();
            };

            APoint->Messages().Send(Message, OnRequest);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SendSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation,
                const CString &Payload) {

            auto OnRequest = [](CHTTPClient *Sender, CHTTPRequest *Request) {

                const auto &uri = Sender->Data()["uri"];
                const auto &payload = Sender->Data()["payload"];

                Request->Content = payload;

                CHTTPRequest::Prepare(Request, "POST", uri.c_str(), "application/soap+xml");

                DebugRequest(Request);
            };

            auto OnExecuteLocal = [AConnection](CTCPConnection *AClientConnection) {

                auto pConnection = dynamic_cast<CHTTPClientConnection *> (AClientConnection);
                auto pClientReply = pConnection->Reply();
                auto pReply = AConnection->Reply();

                DebugReply(pClientReply);

                pReply->ContentType = CHTTPReply::json;

                CJSON Json;
                CSOAPProtocol::SOAPToJSON(pClientReply->Content, Json);

                pReply->Content = Json.ToString();
                AConnection->SendReply(pReply->Content.IsEmpty() ? CHTTPReply::no_content : CHTTPReply::ok, "application/json", true);

                DebugReply(pReply);

                pConnection->CloseConnection(true);
                return true;
            };

            auto OnExecute = [this, AConnection](CTCPConnection *AClientConnection) {

                auto pConnection = dynamic_cast<CHTTPClientConnection *> (AClientConnection);
                auto pReply = pConnection->Reply();

                DebugReply(pReply);

#ifdef WITH_POSTGRESQL
                SOAPToJSON(AConnection, pReply->Content);
#else
                AConnection->SendStockReply(CHTTPReply::not_implemented);
#endif
                pConnection->CloseConnection(true);
                return true;
            };

            auto OnException = [](CTCPConnection *AClientConnection, const Delphi::Exception::Exception &E) {

                auto pConnection = dynamic_cast<CHTTPClientConnection *> (AClientConnection);
                auto pClient = dynamic_cast<CHTTPClient *> (pConnection->Client());

                Log()->Error(APP_LOG_ERR, 0, "[%s:%d] %s", pClient->Host().c_str(), pClient->Port(), E.what());
            };

            CLocation uri(APoint->Address());

            auto pClient = GetClient(uri.hostname, uri.port);

            pClient->Data().Values("uri", uri.href());
            pClient->Data().Values("payload", Payload);

            pClient->OnRequest(OnRequest);
#ifdef WITH_POSTGRESQL
            pClient->OnExecute(OnExecute);
#else
            pClient->OnExecute(OnExecuteLocal);
#endif
            pClient->OnException(OnException);

            pClient->Active(true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();

            CString sPath(pRequest->Location.pathname);

            // Request path must be absolute and not contain "..".
            if (sPath.empty() || sPath.front() != '/' || sPath.find(_T("..")) != CString::npos) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            if (sPath.SubString(0, 6) == "/ocpp/") {
                DoOCPP(AConnection);
                return;
            }

            if (sPath.SubString(0, 5) == "/api/") {
                DoAPI(AConnection);
                return;
            }

            SendResource(AConnection, sPath);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPost(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();

            CStringList slPath;
            SplitColumns(pRequest->Location.pathname, slPath, '/');

            if (slPath.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& caService = slPath[0];

            if (caService == "api") {
                DoAPI(AConnection);
                return;
            } else if (caService == "Ocpp") {
                DoSOAP(AConnection);
                return;
            }

            AConnection->SendStockReply(CHTTPReply::not_found);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoChargePoint(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Operation) {

            auto pRequest = AConnection->Request();

            const auto index = m_Operations.IndexOfName(Operation);
            if (index == -1) {
                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid operation: %s", Operation.c_str()));
                return;
            }

            auto pPoint = m_PointManager.FindPointByIdentity(Identity);

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
            ContentToJson(pRequest, Payload);

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
                if (caValue.IsNull() || caValue.IsEmpty()) {
                    Object.Delete(i);
                }
            }

            for (int i = 0; i < caFields.Count(); i++) {
                const auto &field = caFields[i];
                if (field.required && Payload[field.name].IsNull()) {
                    ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found required key: %s (%s)", field.name.c_str(), field.type.c_str()));
                    return;
                }
            }

            auto pConnection = pPoint->Connection();

            if (pConnection != nullptr && pConnection->Protocol() == pWebSocket) {

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

        void CCSService::DoAuthChargePointList(const CAuthorization &Authorization, CHTTPServerConnection *AConnection) {
#ifdef WITH_POSTGRESQL
            auto OnExecuted = [this](CPQPollQuery *APollQuery) {

                CPQResult *pResult;

                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (pConnection != nullptr && !pConnection->ClosedGracefully()) {
                    try {
                        auto pReply = pConnection->Reply();

                        for (int i = 0; i < APollQuery->Count(); i++) {
                            pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            pReply->Content = pResult->GetValue(0, 0);
                            pConnection->SendReply(CHTTPReply::ok, nullptr, true);
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ReplyError(pConnection, CHTTPReply::bad_request, E.what());
                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }
                }
            };

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                ReplyError(pConnection, CHTTPReply::internal_server_error, E.what());
            };

            CStringList SQL;

            const auto &caPayload = GetChargePointList().ToString();

            SQL.Add(CString()
                            .MaxFormatSize(256 + Authorization.Token.Size() + caPayload.Size())
                            .Format("SELECT * FROM ocpp.ChargePointList(%s, %s::jsonb);",
                                    PQQuoteLiteral(Authorization.Token).c_str(),
                                    PQQuoteLiteral(caPayload).c_str()
                            )
            );

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, E.what());
            }
#else
            auto pReply = AConnection->Reply();
            pReply->Content = GetChargePointList().ToString();
            AConnection->SendReply(CHTTPReply::ok);
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoChargePointList(CHTTPServerConnection *AConnection) {
            auto pReply = AConnection->Reply();
            pReply->Content = GetChargePointList().ToString();
            AConnection->SendReply(CHTTPReply::ok);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoAPI(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            CStringList slPath;
            SplitColumns(pRequest->Location.pathname, slPath, '/');

            if (slPath.Count() == 0) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto &caService = slPath[0];

            if (slPath.Count() < 3) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto &caVersion = slPath[1];
            const auto &caCommand = slPath[2];

            try {
                if (caVersion == "v1") {
                    pReply->ContentType = CHTTPReply::json;

                    if (caCommand == "ping") {
                        AConnection->SendStockReply(CHTTPReply::ok);
                        return;
                    } else if (caCommand == "time") {
                        pReply->Content << "{\"serverTime\": " << LongToString(MsEpoch()) << "}";
                        AConnection->SendReply(CHTTPReply::ok);
                        return;
                    } else {
#ifdef WITH_AUTHORIZATION
                        CAuthorization Authorization;
                        if (!CheckAuthorization(AConnection, Authorization)) {
                            return;
                        }

                        if (caCommand == "ChargePointList") {
                            DoAuthChargePointList(Authorization, AConnection);
                            return;
#else
                        if (caCommand == "ChargePointList") {
                            DoChargePointList(AConnection);
                            return;
#endif
                        } else if (caCommand == "ChargePoint" && slPath.Count() >= 5) {
                            DoChargePoint(AConnection, slPath[3], slPath[4]);
                            return;
                        }
                    }
                }

                AConnection->SendStockReply(CHTTPReply::not_found);
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoSOAP(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

#ifdef WITH_POSTGRESQL
            // Let's process the request in the DBMS
            ParseSOAP(AConnection, pRequest->Content);
#else
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

            pPoint->Parse(pRequest->Content, pReply->Content);

            AConnection->SendReply(CHTTPReply::ok, "application/soap+xml");
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoWebSocket(CHTTPServerConnection *AConnection) {
            try {
                auto pPoint = dynamic_cast<CCSChargingPoint *> (CCSChargingPoint::FindOfConnection(AConnection));
                auto pWSRequest = AConnection->WSRequest();

                CString Request(pWSRequest->Payload());
#ifdef WITH_POSTGRESQL
                while (Request.Position() < Request.Size()) {
                    CJSONMessage Message;
                    size_t size = CJSONProtocol::Request(Request, Message);

                    if (size == 0)
                        throw ExceptionFrm("Invalid request data: %s", Request.c_str());

                    Request.Position(size);

                    if (Message.MessageTypeId == ChargePoint::mtCall) {
                        // Let's process the request in the DBMS
                        ParseJSON(AConnection, pPoint->Identity(), Message);
                    } else {
                        // We will send the response from the charging station to the handler
                        auto pHandler = pPoint->Messages().FindMessageById(Message.UniqueId);

                        if (Assigned(pHandler)) {
                            pHandler->Handler(AConnection);
                            delete pHandler;
                        }

                        pWSRequest->Clear();
                    }
                }
#else
                auto pWSReply = AConnection->WSReply();

                CString Response;

                if (!pPoint->Parse(Request, Response))
                    throw ExceptionFrm("Unknown WebSocket request: %s", Request.c_str());

                if (!Response.IsEmpty()) {
                    pWSReply->SetPayload(Response);
                    AConnection->SendWebSocket();
                }
#endif
            } catch (std::exception &e) {
                AConnection->Clear();
                Log()->Error(APP_LOG_ERR, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoOCPP(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            pReply->ContentType = CHTTPReply::html;

            CStringList slPath;
            SplitColumns(pRequest->Location.pathname, slPath, '/');

            if (slPath.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& caSecWebSocketKey = pRequest->Headers.Values("sec-websocket-key");
            if (caSecWebSocketKey.IsEmpty()) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            const auto& caIdentity = slPath[1];
            const CString csAccept(SHA1(caSecWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            const auto& caSecWebSocketProtocol = pRequest->Headers.Values("sec-websocket-protocol");
            const CString csProtocol(caSecWebSocketProtocol.IsEmpty() ? "" : caSecWebSocketProtocol.SubString(0, caSecWebSocketProtocol.Find(',')));

            AConnection->SwitchingProtocols(csAccept, csProtocol);

            auto pPoint = m_PointManager.FindPointByIdentity(caIdentity);

            if (pPoint == nullptr) {
                pPoint = m_PointManager.Add(AConnection);
                pPoint->ProtocolType(ChargePoint::ptJSON);
                pPoint->Identity() = caIdentity;
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

        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Initialization(CModuleProcess *AProcess) {
            CApostolModule::Initialization(AProcess);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Heartbeat() {
            CApostolModule::Heartbeat();
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::Execute(CHTTPServerConnection *AConnection) {
            if (AConnection->Protocol() == pWebSocket) {
                DoWebSocket(AConnection);
                return true;
            } else {
                return CApostolModule::Execute(AConnection);
            }
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