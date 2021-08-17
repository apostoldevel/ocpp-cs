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

            m_bParseInDataBase = false;

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
            
            m_Operations.AddPair("CancelReservation", CStringList());
            m_Operations.Last().Value().Add("reservationId=true");

            m_Operations.AddPair("ChangeAvailability", CStringList());
            m_Operations.Last().Value().Add("connectorId=true");
            m_Operations.Last().Value().Add("type=true");

            m_Operations.AddPair("ChangeConfiguration", CStringList());
            m_Operations.Last().Value().Add("key=true");
            m_Operations.Last().Value().Add("value=true");

            m_Operations.AddPair("ClearCache", CStringList());

            m_Operations.AddPair("ClearChargingProfile", CStringList());
            m_Operations.Last().Value().Add("id=false");
            m_Operations.Last().Value().Add("connectorId=false");
            m_Operations.Last().Value().Add("chargingProfilePurpose=false");
            m_Operations.Last().Value().Add("stackLevel=false");

            m_Operations.AddPair("DataTransfer", CStringList());
            m_Operations.Last().Value().Add("vendorId=true");
            m_Operations.Last().Value().Add("messageId=false");
            m_Operations.Last().Value().Add("data=false");

            m_Operations.AddPair("GetCompositeSchedule", CStringList());
            m_Operations.Last().Value().Add("connectorId=true");
            m_Operations.Last().Value().Add("duration=true");
            m_Operations.Last().Value().Add("chargingRateUnit=false");

            m_Operations.AddPair("GetConfiguration", CStringList());
            m_Operations.Last().Value().Add("key=false");

            m_Operations.AddPair("GetDiagnostics", CStringList());
            m_Operations.Last().Value().Add("location=true");
            m_Operations.Last().Value().Add("retries=false");
            m_Operations.Last().Value().Add("retryInterval=false");
            m_Operations.Last().Value().Add("startTime=false");
            m_Operations.Last().Value().Add("stopTime=false");

            m_Operations.AddPair("GetLocalListVersion", CStringList());

            m_Operations.AddPair("RemoteStartTransaction", CStringList());
            m_Operations.Last().Value().Add("connectorId=false");
            m_Operations.Last().Value().Add("idTag=true");
            m_Operations.Last().Value().Add("chargingProfile=false");

            m_Operations.AddPair("RemoteStopTransaction", CStringList());
            m_Operations.Last().Value().Add("transactionId=true");

            m_Operations.AddPair("ReserveNow", CStringList());
            m_Operations.Last().Value().Add("connectorId=true");
            m_Operations.Last().Value().Add("expiryDate=true");
            m_Operations.Last().Value().Add("idTag=true");
            m_Operations.Last().Value().Add("parentIdTag=false");
            m_Operations.Last().Value().Add("reservationId=true");

            m_Operations.AddPair("Reset", CStringList());
            m_Operations.Last().Value().Add("type=true");

            m_Operations.AddPair("SendLocalList", CStringList());
            m_Operations.Last().Value().Add("listVersion=true");
            m_Operations.Last().Value().Add("localAuthorizationList=false");
            m_Operations.Last().Value().Add("updateType=true");

            m_Operations.AddPair("SetChargingProfile", CStringList());
            m_Operations.Last().Value().Add("connectorId=true");
            m_Operations.Last().Value().Add("csChargingProfiles=true");

            m_Operations.AddPair("TriggerMessage", CStringList());
            m_Operations.Last().Value().Add("requestedMessage=true");
            m_Operations.Last().Value().Add("connectorId=false");

            m_Operations.AddPair("UnlockConnector", CStringList());
            m_Operations.Last().Value().Add("connectorId=true");

            m_Operations.AddPair("UpdateFirmware", CStringList());
            m_Operations.Last().Value().Add("location=true");
            m_Operations.Last().Value().Add("retries=false");
            m_Operations.Last().Value().Add("retrieveDate=true");
            m_Operations.Last().Value().Add("retryInterval=false");
        }
        //--------------------------------------------------------------------------------------------------------------

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

        void CCSService::SetPointConnected(CChargingPoint *APoint, bool Value) {

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

        void CCSService::DoPointConnected(CChargingPoint *APoint) {
            if (m_bParseInDataBase && APoint->UpdateConnected())
                SetPointConnected(APoint, true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPointDisconnected(CObject *Sender) {
            auto pConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (pConnection != nullptr) {

                auto pSocket = pConnection->Socket();

                try {
                    auto pPoint = CChargingPoint::FindOfConnection(pConnection);

                    if (pPoint != nullptr) {
                        if (m_bParseInDataBase && pPoint->UpdateConnected()) {
                            SetPointConnected(pPoint, false);
                        }

                        if (pSocket != nullptr) {
                            auto pHandle = pSocket->Binding();
                            if (pHandle != nullptr) {
                                Log()->Message(_T("[%s:%d] Point \"%s\" closed connection."),
                                               pHandle->PeerIP(), pHandle->PeerPort(),
                                               pPoint->Identity().IsEmpty() ? "(empty)" : pPoint->Identity().c_str()
                                               );
                            }
                        } else {
                            Log()->Message(_T("Point \"%s\" closed connection."),
                                           pPoint->Identity().IsEmpty() ? "(empty)" : pPoint->Identity().c_str());
                        }

                        if (pPoint->UpdateCount() == 0) {
                            delete pPoint;
                        }
                    } else {
                        if (pSocket != nullptr) {
                            auto pHandle = pSocket->Binding();
                            if (pHandle != nullptr) {
                                Log()->Message(_T("[%s:%d] Unknown point closed connection."),
                                               pHandle->PeerIP(), pHandle->PeerPort()
                                               );
                            }
                        } else {
                            Log()->Message(_T("Unknown point closed connection."));
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

                jmResponse.MessageTypeId = OCPP::mtCallError;
                jmResponse.ErrorCode = "InternalError";
                jmResponse.ErrorDescription = E.what();

                CJSONProtocol::Response(jmResponse, sResponse);

                pWSReply->SetPayload(sResponse);
                AConnection->SendWebSocket(true);
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
                                pPoint->ProtocolType(OCPP::ptSOAP);
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

            SQL.Add(CString().Format("SELECT * FROM ocpp.ParseXML('%s'::xml);", Payload.c_str()));

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                SOAPError(AConnection, "Receiver", "InternalError", "ExecSQL call failed.", E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::ParseJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Action,
                const CJSON &Payload) {

            auto OnExecuted = [](CPQPollQuery *APollQuery) {

                CPQResult *pResult;

                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());

                if (pConnection != nullptr && !pConnection->ClosedGracefully()) {

                    auto pWSRequest = pConnection->WSRequest();
                    auto pWSReply = pConnection->WSReply();

                    const CString csRequest(pWSRequest->Payload());

                    CJSONMessage jmRequest;
                    CJSONProtocol::Request(csRequest, jmRequest);

                    CJSONMessage jmResponse;
                    CString sResponse;

                    CJSON jResult;

                    CJSONProtocol::PrepareResponse(jmRequest, jmResponse);

                    try {
                        for (int i = 0; i < APollQuery->Count(); i++) {
                            pResult = APollQuery->Results(i);

                            if (pResult->ExecStatus() != PGRES_TUPLES_OK)
                                throw Delphi::Exception::EDBError(pResult->GetErrorMessage());

                            jResult << pResult->GetValue(0, 0);

                            const auto &caResponse = jResult["response"];

                            if (!jResult["result"].AsBoolean()) {
                                jmResponse.Payload << caResponse["error"].ToString();
                                throw Delphi::Exception::EDBError("Database query execution.");
                            }

                            jmResponse.Payload << caResponse.ToString();
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        jmResponse.MessageTypeId = OCPP::mtCallError;
                        jmResponse.ErrorCode = "InternalError";
                        jmResponse.ErrorDescription = E.what();

                        Log()->Error(APP_LOG_ERR, 0, E.what());
                    }

                    CJSONProtocol::Response(jmResponse, sResponse);

                    pWSReply->SetPayload(sResponse);
                    pConnection->SendWebSocket(true);
                }
            };

            auto OnException = [](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                auto pConnection = dynamic_cast<CHTTPServerConnection *>(APollQuery->Binding());
                DoWebSocketError(pConnection, E);
            };

            CStringList SQL;

            SQL.Add(CString().Format("SELECT * FROM ocpp.Parse('%s', '%s', '%s'::jsonb);", Identity.c_str(), Action.c_str(), Payload.ToString().c_str()));

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                DoWebSocketError(AConnection, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

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

            const auto Index = OAuth2::Helper::ProviderByClientId(Providers, aud, Application);
            if (Index == -1)
                throw COAuth2Error(_T("Not found provider by Client ID."));

            const auto& Provider = Providers[Index].Value();

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

        void CCSService::SendJSON(CHTTPServerConnection *AConnection, CChargingPoint *APoint, const CString &Operation,
                const CJSON &Payload) {

            auto OnRequest = [AConnection](OCPP::CMessageHandler *AHandler, CHTTPServerConnection *AWSConnection) {

                auto pWSRequest = AWSConnection->WSRequest();

                if (!AConnection->ClosedGracefully()) {

                    auto pReply = AConnection->Reply();
                    const CString pRequest(pWSRequest->Payload());

                    CHTTPReply::CStatusType Status = CHTTPReply::ok;

                    try {
                        CJSONMessage jsonMessage;

                        if (!CJSONProtocol::Request(pRequest, jsonMessage)) {
                            Status = CHTTPReply::bad_request;
                        }

                        if (jsonMessage.MessageTypeId == OCPP::mtCallError) {
                            ReplyError(AConnection, CHTTPReply::bad_request,
                                       CString().Format("%s: %s",
                                                        jsonMessage.ErrorCode.c_str(),
                                                        jsonMessage.ErrorDescription.c_str()
                                                        ));
                        } else {
                            pReply->Content = jsonMessage.Payload.ToString();
                            AConnection->SendReply(Status, "application/json", true);
                        }
                    } catch (std::exception &e) {
                        ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
                    }
                }

                AWSConnection->ConnectionStatus(csReplySent);
                pWSRequest->Clear();
            };

            APoint->Messages()->Add(OnRequest, Operation, Payload);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::SendSOAP(CHTTPServerConnection *AConnection, CChargingPoint *APoint, const CString &Operation,
                const CString &Payload) {

            auto OnRequest = [](CHTTPClient *Sender, CHTTPRequest *Request) {

                const auto &uri = Sender->Data()["uri"];
                const auto &payload = Sender->Data()["payload"];

                Request->Content = payload;

                CHTTPRequest::Prepare(Request, "POST", uri.c_str(), "application/soap+xml");

                DebugRequest(Request);
            };

            auto OnExecute = [AConnection](CTCPConnection *AClientConnection) {

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

            auto OnExecuteDB = [this, AConnection](CTCPConnection *AClientConnection) {

                auto pConnection = dynamic_cast<CHTTPClientConnection *> (AClientConnection);
                auto pReply = pConnection->Reply();

                DebugReply(pReply);

                SOAPToJSON(AConnection, pReply->Content);

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
            pClient->OnExecute(OnExecute);
            pClient->OnException(OnException);

            pClient->Active(true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::JSONToSOAP(CHTTPServerConnection *AConnection, CChargingPoint *APoint, const CString &Operation,
            const CJSON &Payload) {

            auto OnExecuted = [this, APoint, &Operation](CPQPollQuery *APollQuery) {

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

                            CString caPayload(R"(<?xml version="1.0" encoding="UTF-8"?>)" LINEFEED);
                            caPayload << pResult->GetValue(0, 0);

                            SendSOAP(pConnection, APoint, Operation, caPayload);
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

            SQL.Add(CString().Format("SELECT * FROM ocpp.JSONToSOAP('%s'::jsonb);", Data.ToString().c_str()));

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

            SQL.Add(CString().Format("SELECT * FROM ocpp.SOAPToJSON('%s'::xml);", Payload.c_str()));

            try {
                ExecSQL(SQL, AConnection, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, E.what());
            }
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
            auto pReply = AConnection->Reply();

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
            auto pReply = AConnection->Reply();

            const auto Index = m_Operations.IndexOfName(Operation);
            if (Index == -1) {
                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid operation: %s", Operation.c_str()));
                return;
            }

            auto pPoint = m_PointManager.FindPointByIdentity(Identity);

            if (pPoint == nullptr) {
                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found Charge Point by Identity: %s", Identity.c_str()));
                return;
            }

            CJSON Json;
            ContentToJson(pRequest, Json);

            const auto &caFields = m_Operations[Index].Value();
            auto &Object = Json.Object();

            for (int i = 0; i < Object.Count(); i++) {

                const auto& caMember = Object.Members(i);
                const auto& caKey = caMember.String();

                if (caFields.IndexOfName(caKey) == -1) {
                    ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid key: %s", caKey.c_str()));
                    return;
                }

                const auto& caValue = caMember.Value();
                if (caValue.IsNull() || caValue.IsEmpty()) {
                    Object.Delete(i);
                }
            }

            for (int i = 0; i < caFields.Count(); i++) {
                const auto& caKey = caFields.Names(i);
                const auto bRequired = caFields.ValueFromIndex(i) == "true";
                if (bRequired && Json[caKey].IsNull()) {
                    ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found required key: %s", caKey.c_str()));
                    return;
                }
            }

            auto pConnection = pPoint->Connection();

            if (pConnection != nullptr && pConnection->Protocol() == pWebSocket) {

                if (!pConnection->Connected()) {
                    ReplyError(AConnection, CHTTPReply::bad_request, "Charge Point not connected.");
                    return;
                }

                SendJSON(AConnection, pPoint, Operation, Json);
            } else {
                JSONToSOAP(AConnection, pPoint, Operation, Json);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoChargePointList(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            CJSONObject jsonObject;
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
                }

                jsonArray.Array().Add(jsonPoint);
            }

            jsonObject.AddPair("ChargePointList", jsonArray);

            pReply->Content = jsonObject.ToString();

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
                    } else if (caCommand == "ChargePointList") {
                        DoChargePointList(AConnection);
                        return;
                    } else if (caCommand == "ChargePoint" && slPath.Count() >= 5) {

                        CAuthorization Authorization;
                        if (!CheckAuthorization(AConnection, Authorization)) {
                            return;
                        }

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

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            if (m_bParseInDataBase) {
                // Обработаем запрос в СУБД
                ParseSOAP(AConnection, pRequest->Content);
            } else {
                auto pPoint = m_PointManager.FindPointByConnection(AConnection);

                if (pPoint == nullptr) {
                    pPoint = m_PointManager.Add(AConnection);
                    pPoint->ProtocolType(OCPP::ptSOAP);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                    AConnection->OnDisconnected([this](auto && Sender) { DoPointDisconnected(Sender); });
#else
                    AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
#endif
                }

                pPoint->Parse(pRequest->Content, pReply->Content);

                AConnection->SendReply(CHTTPReply::ok, "application/soap+xml");
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
                pPoint->ProtocolType(OCPP::ptJSON);
                pPoint->Identity() = caIdentity;
            } else {
                pPoint->UpdateConnected(false);
                pPoint->SwitchConnection(AConnection);
            }

            pPoint->Address() = GetHost(AConnection);

            pPoint->UpdateConnected(true);
            DoPointConnected(pPoint);

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            AConnection->OnDisconnected([this](auto && Sender) { DoPointDisconnected(Sender); });
#else
            AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoWebSocket(CHTTPServerConnection *AConnection) {

            auto pWSRequest = AConnection->WSRequest();
            auto pWSReply = AConnection->WSReply();

            const CString csRequest(pWSRequest->Payload());

            try {
                auto pPoint = CChargingPoint::FindOfConnection(AConnection);

                if (m_bParseInDataBase) {

                    CJSONMessage jmRequest;
                    CJSONProtocol::Request(csRequest, jmRequest);

                    if (jmRequest.MessageTypeId == OCPP::mtCall) {
                        // Обработаем запрос в СУБД
                        ParseJSON(AConnection, pPoint->Identity(), jmRequest.Action, jmRequest.Payload);
                    } else {
                        // Ответ от зарядной станции отправим в обработчик
                        auto pHandler = pPoint->Messages()->FindMessageById(jmRequest.UniqueId);
                        if (Assigned(pHandler)) {
                            pHandler->Handler(AConnection);
                            delete pHandler;
                        }
                        pWSRequest->Clear();
                    }

                } else {

                    CString sResponse;

                    if (pPoint->Parse(csRequest, sResponse)) {
                        if (!sResponse.IsEmpty()) {
                            pWSReply->SetPayload(sResponse);
                            AConnection->SendWebSocket();
                        }
                    } else {
                        Log()->Error(APP_LOG_ERR, 0, "Unknown WebSocket request: %s", csRequest.c_str());
                    }
                }
            } catch (std::exception &e) {
                //AConnection->SendWebSocketClose();
                AConnection->Clear();
                Log()->Error(APP_LOG_ERR, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Initialization(CModuleProcess *AProcess) {
            CApostolModule::Initialization(AProcess);
            m_bParseInDataBase = Config()->PostgresConnect();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Heartbeat() {
            CApostolModule::Heartbeat();
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::Execute(CHTTPServerConnection *AConnection) {
            if (AConnection->Protocol() == pWebSocket) {
#ifdef _DEBUG
                WSDebugConnection(AConnection);
#endif
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