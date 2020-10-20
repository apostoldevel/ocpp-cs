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
            m_CPManager = new CChargingPointManager();
            m_Headers.Add("Authorization");

            m_ParseInDataBase = false;

            CCSService::InitMethods();
            InitOperations();
        }
        //--------------------------------------------------------------------------------------------------------------

        CCSService::~CCSService() {
            delete m_CPManager;
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
                Log()->Error(APP_LOG_EMERG, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
            Log()->Error(APP_LOG_EMERG, 0, E.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPointDisconnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (LConnection != nullptr) {
                auto LPoint = CChargingPoint::FindOfConnection(LConnection);
                if (LPoint != nullptr) {
                    if (!LConnection->ClosedGracefully()) {
                        Log()->Message(_T("[%s:%d] Point \"%s\" closed connection."),
                                       LConnection->Socket()->Binding()->PeerIP(),
                                       LConnection->Socket()->Binding()->PeerPort(),
                                       LPoint->Identity().IsEmpty() ? "(empty)" : LPoint->Identity().c_str()
                        );
                    }
                    if (LPoint->UpdateCount() == 0) {
                        delete LPoint;
                    }
                } else {
                    if (!LConnection->ClosedGracefully()) {
                        Log()->Message(_T("[%s:%d] Unknown point closed connection."),
                                       LConnection->Socket()->Binding()->PeerIP(),
                                       LConnection->Socket()->Binding()->PeerPort()
                        );
                    }
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoWebSocketError(CHTTPServerConnection *AConnection, const Delphi::Exception::Exception &E) {

            Log()->Error(APP_LOG_EMERG, 0, E.what());

            if (AConnection->ClosedGracefully())
                return;

            auto LWSRequest = AConnection->WSRequest();
            auto LWSReply = AConnection->WSReply();

            const CString LRequest(LWSRequest->Payload());

            CJSONMessage jmRequest;
            CJSONProtocol::Request(LRequest, jmRequest);

            CJSONMessage jmResponse;
            CString LResponse;

            CJSON LResult;

            CJSONProtocol::PrepareResponse(jmRequest, jmResponse);

            jmResponse.MessageTypeId = OCPP::mtCallError;
            jmResponse.ErrorCode = "InternalError";
            jmResponse.ErrorDescription = E.what();

            CJSONProtocol::Response(jmResponse, LResponse);

            LWSReply->SetPayload(LResponse);
            AConnection->SendWebSocket(true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::ParseJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Action,
                const CJSON &Payload) {

            auto OnExecuted = [AConnection](CPQPollQuery *APollQuery) {

                if (AConnection->ClosedGracefully())
                    return;

                CPQResult *Result;

                auto LWSRequest = AConnection->WSRequest();
                auto LWSReply = AConnection->WSReply();

                const CString LRequest(LWSRequest->Payload());

                CJSONMessage jmRequest;
                CJSONProtocol::Request(LRequest, jmRequest);

                CJSONMessage jmResponse;
                CString LResponse;

                CJSON LResult;

                CJSONProtocol::PrepareResponse(jmRequest, jmResponse);

                try {
                    for (int I = 0; I < APollQuery->Count(); I++) {
                        Result = APollQuery->Results(I);

                        if (Result->ExecStatus() != PGRES_TUPLES_OK)
                            throw Delphi::Exception::EDBError(Result->GetErrorMessage());

                        LResult << Result->GetValue(0, 0);

                        const auto& Response = LResult["response"];

                        if (!LResult["result"].AsBoolean())
                            throw Delphi::Exception::EDBError(Response["error"].AsString().c_str());

                        jmResponse.Payload << Response.ToString();
                    }
                } catch (Delphi::Exception::Exception &E) {
                    jmResponse.MessageTypeId = OCPP::mtCallError;
                    jmResponse.ErrorCode = "InternalError";
                    jmResponse.ErrorDescription = E.what();

                    Log()->Error(APP_LOG_EMERG, 0, E.what());
                }

                CJSONProtocol::Response(jmResponse, LResponse);

                LWSReply->SetPayload(LResponse);
                AConnection->SendWebSocket(true);
            };

            auto OnException = [AConnection](CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
                DoWebSocketError(AConnection, E);
            };

            CStringList SQL;

            SQL.Add(CString());
            SQL.Last().Format("SELECT * FROM ocpp.Parse('%s', '%s', '%s'::jsonb);", Identity.c_str(), Action.c_str(), Payload.ToString().c_str());

            try {
                ExecSQL(SQL, nullptr, OnExecuted, OnException);
            } catch (Delphi::Exception::Exception &E) {
                DoWebSocketError(AConnection, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::VerifyToken(const CString &Token) {

            const auto& GetSecret = [](const CProvider &Provider, const CString &Application) {
                const auto &Secret = Provider.Secret(Application);
                if (Secret.IsEmpty())
                    throw ExceptionFrm("Not found Secret for \"%s:%s\"",
                                       Provider.Name.c_str(),
                                       Application.c_str()
                    );
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
            const CStringList& Issuers = Provider.GetIssuers(Application);
            if (Issuers[iss].IsEmpty())
                throw jwt::token_verification_exception("Token doesn't contain the required issuer.");

            const auto& alg = decoded.get_algorithm();

            const auto& Secret = GetSecret(Provider, Application);

            if (alg == "HS256") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{Secret});
                verifier.verify(decoded);
            } else if (alg == "HS384") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs384{Secret});
                verifier.verify(decoded);
            } else if (alg == "HS512") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs512{Secret});
                verifier.verify(decoded);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization) {

            const auto &LHeaders = ARequest->Headers;
            const auto &LCookies = ARequest->Cookies;

            const auto &LAuthorization = LHeaders.Values(_T("Authorization"));

            if (LAuthorization.IsEmpty()) {

                const auto &headerSession = LHeaders.Values(_T("Session"));
                const auto &headerSecret = LHeaders.Values(_T("Secret"));

                Authorization.Username = headerSession;
                Authorization.Password = headerSecret;

                if (Authorization.Username.IsEmpty() || Authorization.Password.IsEmpty())
                    return false;

                Authorization.Schema = CAuthorization::asBasic;
                Authorization.Type = CAuthorization::atSession;

            } else {
                Authorization << LAuthorization;
            }

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization) {

            auto LRequest = AConnection->Request();

            try {
                if (CheckAuthorizationData(LRequest, Authorization)) {
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

        void CCSService::DoOCPP(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            LReply->ContentType = CHTTPReply::html;

            CStringList LPath;
            SplitColumns(LRequest->Location.pathname, LPath, '/');

            if (LPath.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const CString& LSecWebSocketKey = LRequest->Headers.Values("sec-websocket-key");
            if (LSecWebSocketKey.IsEmpty()) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            const auto& LIdentity = LPath[1];
            const CString LAccept(SHA1(LSecWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            const auto& LSecWebSocketProtocol = LRequest->Headers.Values("sec-websocket-protocol");
            const CString LProtocol(LSecWebSocketProtocol.IsEmpty() ? "" : LSecWebSocketProtocol.SubString(0, LSecWebSocketProtocol.Find(',')));

            AConnection->SwitchingProtocols(LAccept, LProtocol);

            auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);

            if (LPoint == nullptr) {
                LPoint = m_CPManager->Add(AConnection);
                LPoint->Identity() = LIdentity;
            } else {
                LPoint->SwitchConnection(AConnection);
            }

            LPoint->Address() = GetHost(AConnection);

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            AConnection->OnDisconnected([this](auto && Sender) { DoPointDisconnected(Sender); });
#else
            AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoAPI(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CStringList LPath;
            SplitColumns(LRequest->Location.pathname, LPath, '/');

            if (LPath.Count() == 0) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto &LService = LPath[0];

            if (LPath.Count() < 3) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto &LVersion = LPath[1];
            const auto &LCommand = LPath[2];

            try {

                if (LVersion == "v1") {

                    LReply->ContentType = CHTTPReply::json;

                    if (LCommand == "ping") {

                        AConnection->SendStockReply(CHTTPReply::ok);
                        return;

                    } else if (LCommand == "time") {

                        LReply->Content << "{\"serverTime\": " << LongToString(MsEpoch()) << "}";
                        AConnection->SendReply(CHTTPReply::ok);
                        return;

                    } else if (LCommand == "ChargePointList") {

                        CJSONObject jsonObject;
                        CJSONValue jsonArray(jvtArray);

                        for (int i = 0; i < m_CPManager->Count(); i++) {
                            CJSONValue jsonPoint(jvtObject);
                            CJSONValue jsonConnection(jvtObject);

                            auto LPoint = m_CPManager->Points(i);

                            jsonPoint.Object().AddPair("Identity", LPoint->Identity());
                            jsonPoint.Object().AddPair("Address", LPoint->Address());

                            if (LPoint->Connection()->Connected()) {
                                jsonConnection.Object().AddPair("Socket", LPoint->Connection()->Socket()->Binding()->Handle());
                                jsonConnection.Object().AddPair("IP", LPoint->Connection()->Socket()->Binding()->PeerIP());
                                jsonConnection.Object().AddPair("Port", LPoint->Connection()->Socket()->Binding()->PeerPort());

                                jsonPoint.Object().AddPair("Connection", jsonConnection);
                            }

                            jsonArray.Array().Add(jsonPoint);
                        }

                        jsonObject.AddPair("ChargePointList", jsonArray);

                        LReply->Content = jsonObject.ToString();

                        AConnection->SendReply(CHTTPReply::ok);

                        return;

                    } else if (LCommand == "ChargePoint") {

                        CAuthorization Authorization;
                        if (!CheckAuthorization(AConnection, Authorization)) {
                            return;
                        }

                        if (LPath.Count() < 5) {
                            AConnection->SendStockReply(CHTTPReply::not_found);
                            return;
                        }

                        const auto &LIdentity = LPath[3];
                        const auto &LOperation = LPath[4];

                        const auto Index = m_Operations.IndexOfName(LOperation);
                        if (Index == -1) {
                            ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid operation: %s", LOperation.c_str()));
                            return;
                        }

                        CJSON Json;
                        ContentToJson(LRequest, Json);

                        const auto &Fields = m_Operations[Index].Value();
                        const auto &Object = Json.Object();

                        for (int i = 0; i < Object.Count(); i++) {
                            const auto& Key = Object.Members(i).String();
                            if (Fields.IndexOfName(Key) == -1) {
                                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Invalid key: %s", Key.c_str()));
                                return;
                            }
                        }

                        for (int i = 0; i < Fields.Count(); i++) {
                            const auto& Key = Fields.Names(i);
                            const auto Required = Fields.ValueFromIndex(i) == "true";
                            if (Required && Json[Key].IsNull()) {
                                ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found required key: %s", Key.c_str()));
                                return;
                            }
                        }

                        auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);

                        if (LPoint == nullptr) {
                            ReplyError(AConnection, CHTTPReply::bad_request, CString().Format("Not found Charge Point by Identity: %s", LIdentity.c_str()));
                            return;
                        }

                        auto LConnection = LPoint->Connection();
                        if (LConnection == nullptr) {
                            ReplyError(AConnection, CHTTPReply::bad_request, "Charge Point offline.");
                            return;
                        }

                        if (!LConnection->Connected()) {
                            ReplyError(AConnection, CHTTPReply::bad_request, "Charge Point not connected.");
                            return;
                        }

                        if (LConnection->Protocol() != pWebSocket) {
                            ReplyError(AConnection, CHTTPReply::bad_request, "Incorrect Charge Point protocol version.");
                            return;
                        }

                        auto OnRequest = [AConnection](OCPP::CMessageHandler *AHandler, CHTTPServerConnection *AWSConnection) {

                            auto LWSRequest = AWSConnection->WSRequest();

                            if (!AConnection->ClosedGracefully()) {

                                auto LReply = AConnection->Reply();
                                const CString LRequest(LWSRequest->Payload());

                                CHTTPReply::CStatusType Status = CHTTPReply::ok;

                                try {
                                    CJSONMessage LMessage;

                                    if (!CJSONProtocol::Request(LRequest, LMessage)) {
                                        Status = CHTTPReply::bad_request;
                                    }

                                    LReply->Content = LMessage.Payload.ToString();
                                    AConnection->SendReply(Status, nullptr, true);
                                } catch (std::exception &e) {
                                    ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
                                }
                            }

                            AWSConnection->ConnectionStatus(csReplySent);
                            LWSRequest->Clear();
                        };

                        LPoint->Messages()->Add(OnRequest, LOperation, Json);

                        return;
                    }
                }

                AConnection->SendStockReply(CHTTPReply::not_found);
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();

            CString LPath(LRequest->Location.pathname);

            // Request path must be absolute and not contain "..".
            if (LPath.empty() || LPath.front() != '/' || LPath.find(_T("..")) != CString::npos) {
                AConnection->SendStockReply(CHTTPReply::bad_request);
                return;
            }

            if (LPath.SubString(0, 6) == "/ocpp/") {
                DoOCPP(AConnection);
                return;
            }

            if (LPath.SubString(0, 5) == "/api/") {
                DoAPI(AConnection);
                return;
            }

            SendResource(AConnection, LPath);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPost(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CStringList LPath;
            SplitColumns(LRequest->Location.pathname, LPath, '/');

            if (LPath.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& LService = LPath[0];

            if (LService == "api") {

                DoAPI(AConnection);
                return;

            } else if (LService == "Ocpp") {

                auto LPoint = m_CPManager->FindPointByConnection(AConnection);
                if (LPoint == nullptr) {
                    LPoint = m_CPManager->Add(AConnection);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                    AConnection->OnDisconnected([this](auto && Sender) { DoPointDisconnected(Sender); });
#else
                    AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
#endif
                }

                LPoint->Parse(ptSOAP, LRequest->Content, LReply->Content);

                AConnection->SendReply(CHTTPReply::ok, "application/soap+xml");

                return;
            }

            AConnection->SendStockReply(CHTTPReply::not_found);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoWebSocket(CHTTPServerConnection *AConnection) {

            auto LWSRequest = AConnection->WSRequest();
            auto LWSReply = AConnection->WSReply();

            const CString LRequest(LWSRequest->Payload());

            try {
                auto LPoint = CChargingPoint::FindOfConnection(AConnection);

                if (m_ParseInDataBase) {

                    CJSONMessage jmRequest;
                    CJSONProtocol::Request(LRequest, jmRequest);

                    if (jmRequest.MessageTypeId == OCPP::mtCall) {
                        // Обработаем запрос в СУБД
                        ParseJSON(AConnection, LPoint->Identity(), jmRequest.Action, jmRequest.Payload);
                    } else {
                        // Ответ от зарядной станции отправим в обработчик
                        auto LHandler = LPoint->Messages()->FindMessageById(jmRequest.UniqueId);
                        if (Assigned(LHandler)) {
                            LHandler->Handler(AConnection);
                            delete LHandler;
                        }
                        LWSRequest->Clear();
                    }

                } else {

                    CString LResponse;

                    if (LPoint->Parse(ptJSON, LRequest, LResponse)) {
                        if (!LResponse.IsEmpty()) {
                            LWSReply->SetPayload(LResponse);
                            AConnection->SendWebSocket();
                        }
                    } else {
                        Log()->Error(APP_LOG_EMERG, 0, "Unknown WebSocket request: %s", LRequest.c_str());
                    }
                }
            } catch (std::exception &e) {
                //AConnection->SendWebSocketClose();
                AConnection->Clear();
                Log()->Error(APP_LOG_EMERG, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Initialization(CModuleProcess *AProcess) {
            CApostolModule::Initialization(AProcess);
            m_ParseInDataBase = Config()->PostgresConnect();
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