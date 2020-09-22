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

            CCSService::InitMethods();
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

        void CCSService::DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->PollConnection());

            if (LConnection != nullptr) {
                auto LWSRequest = LConnection->WSRequest();
                auto LWSReply = LConnection->WSReply();

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
#ifdef _DEBUG
                DebugMessage("\n[%p] [%s:%d] [%d] [WebSocket] Response:\n%s\n", LConnection, LConnection->Socket()->Binding()->PeerIP(),
                             LConnection->Socket()->Binding()->PeerPort(), LConnection->Socket()->Binding()->Handle(), LResponse.c_str());
#endif
                LWSReply->SetPayload(LResponse);
                LConnection->SendWebSocket(true);
            }

            Log()->Error(APP_LOG_EMERG, 0, E.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPostgresQueryExecuted(CPQPollQuery *APollQuery) {
            clock_t start = clock();

            auto LConnection = dynamic_cast<CHTTPServerConnection *> (APollQuery->PollConnection());

            if (LConnection != nullptr) {
                CPQResult *Result;

                auto LWSRequest = LConnection->WSRequest();
                auto LWSReply = LConnection->WSReply();

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
#ifdef _DEBUG
                DebugMessage("\n[%p] [%s:%d] [%d] [WebSocket] Response:\n%s\n", LConnection, LConnection->Socket()->Binding()->PeerIP(),
                             LConnection->Socket()->Binding()->PeerPort(), LConnection->Socket()->Binding()->Handle(), LResponse.c_str());
#endif
                LWSReply->SetPayload(LResponse);
                LConnection->SendWebSocket(true);
            }

            log_debug1(APP_LOG_DEBUG_CORE, Log(), 0, _T("Query executed runtime: %.2f ms."), (double) ((clock() - start) / (double) CLOCKS_PER_SEC * 1000));
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::QueryStart(CHTTPServerConnection *AConnection, const CStringList& SQL) {
            auto LQuery = GetQuery(AConnection);

            if (LQuery == nullptr)
                throw Delphi::Exception::Exception("QueryStart: GetQuery() failed!");

            LQuery->SQL() = SQL;

            if (LQuery->Start() != POLL_QUERY_START_ERROR) {
                AConnection->CloseConnection(false);
                return true;
            } else {
                delete LQuery;
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::DBParse(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Action,
                                 const CJSON &Payload) {
            CStringList SQL;

            SQL.Add(CString());
            SQL.Last().Format("SELECT * FROM ocpp.Parse('%s', '%s', '%s'::jsonb);",
                              Identity.c_str(), Action.c_str(), Payload.ToString().c_str());

            return QueryStart(AConnection, SQL);
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
                                jsonConnection.Object().AddPair("Socket",
                                                                LPoint->Connection()->Socket()->Binding()->Handle());
                                jsonConnection.Object().AddPair("IP",
                                                                LPoint->Connection()->Socket()->Binding()->PeerIP());
                                jsonConnection.Object().AddPair("Port",
                                                                LPoint->Connection()->Socket()->Binding()->PeerPort());
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
                        const auto &LAction = LPath[4];

                        auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);

                        if (LPoint == nullptr) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 300,
                                                   "Charge point not found.");
                            AConnection->SendReply(CHTTPReply::ok);
                            return;
                        }

                        auto LConnection = LPoint->Connection();
                        if (LConnection == nullptr) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 301,
                                                   "Charge point offline.");
                            AConnection->SendReply(CHTTPReply::ok);
                            return;
                        }

                        if (!LConnection->Connected()) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 302,
                                                   "Charge point not connected.");
                            AConnection->SendReply(CHTTPReply::ok);
                            return;
                        }

                        if (LConnection->Protocol() != pWebSocket) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 303,
                                                   "Incorrect charge point protocol version.");
                            AConnection->SendReply(CHTTPReply::ok);
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
                                    Log()->Error(APP_LOG_EMERG, 0, e.what());
                                }

                                AConnection->CloseConnection(true);
                            }

                            AWSConnection->ConnectionStatus(csReplySent);
                            LWSRequest->Clear();
                        };

                        if (LAction == "GetConfiguration") {

                            CJSON LPayload;
                            CJSONArray LArray;

                            const auto &LKey = LRequest->Params["key"];
                            if (!LKey.IsEmpty()) {
                                LArray.Add(LKey);
                                LPayload.Object().AddPair("key", LArray);
                            }

                            LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                            return;
                        }
                    }
                }

                AConnection->SendStockReply(CHTTPReply::not_found);
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::internal_server_error, e.what());
                Log()->Error(APP_LOG_EMERG, 0, e.what());
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

                if (LPath.Count() < 3) {
                    AConnection->SendStockReply(CHTTPReply::not_found);
                    return;
                }

                const auto& LVersion = LPath[1];
                const auto& LCommand = LPath[2];

                if (LVersion == "v1") {

                    LReply->ContentType = CHTTPReply::json;

                    try {
                        CAuthorization Authorization;
                        if (!CheckAuthorization(AConnection, Authorization)) {
                            return;
                        }

                        if (LCommand == "ChargePoint") {

                            if (LPath.Count() < 5) {
                                AConnection->SendStockReply(CHTTPReply::not_found);
                                return;
                            }

                            const auto &LIdentity = LPath[3];
                            const auto &LAction = LPath[4];

                            auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);

                            if (LPoint == nullptr) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 300,
                                                       "Charge point not found.");
                                AConnection->SendReply(CHTTPReply::ok);
                                return;
                            }

                            auto LConnection = LPoint->Connection();
                            if (LConnection == nullptr) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 301,
                                                       "Charge point offline.");
                                AConnection->SendReply(CHTTPReply::ok);
                                return;
                            }

                            if (!LConnection->Connected()) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 302,
                                                       "Charge point not connected.");
                                AConnection->SendReply(CHTTPReply::ok);
                                return;
                            }

                            if (LConnection->Protocol() != pWebSocket) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 303,
                                                       "Incorrect charge point protocol version.");
                                AConnection->SendReply(CHTTPReply::ok);
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
                                        Log()->Error(APP_LOG_EMERG, 0, e.what());
                                    }
                                }

                                AWSConnection->ConnectionStatus(csReplySent);
                                LWSRequest->Clear();
                            };

                            CJSON LPayload;

                            ContentToJson(LRequest, LPayload);

                            if (LAction == "ChangeConfiguration") {

                                const auto &key = LPayload["key"].AsString();
                                const auto &value = LPayload["value"].AsString();

                                if (key.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                if (value.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "RemoteStartTransaction") {

                                const auto &connectorId = LPayload["connectorId"].AsString();
                                const auto &idTag = LPayload["idTag"].AsString();

                                if (connectorId.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                if (idTag.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "RemoteStopTransaction") {

                                const auto &transactionId = LPayload["transactionId"].AsString();

                                if (transactionId.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "ReserveNow") {

                                const auto &connectorId = LPayload["connectorId"].AsString();
                                const auto &expiryDate = LPayload["expiryDate"].AsString();
                                const auto &idTag = LPayload["idTag"].AsString();
                                const auto &parentIdTag = LPayload["parentIdTag"].AsString();
                                const auto &reservationId = LPayload["reservationId"].AsString();

                                if (connectorId.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                if (expiryDate.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                if (idTag.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                if (reservationId.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "CancelReservation") {

                                const auto &reservationId = LPayload["reservationId"].AsString();

                                if (reservationId.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            } else if (LAction == "Reset") {

                                const auto &type = LPayload["type"].AsString();

                                if (type.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            } else if (LAction == "TriggerMessage") {

                                const auto &requestedMessage = LPayload["requestedMessage"].AsString();

                                if (requestedMessage.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            } else if (LAction == "UnlockConnector") {

                                const auto &connectorId = LPayload["connectorId"].AsString();

                                if (connectorId.IsEmpty()) {
                                    AConnection->SendStockReply(CHTTPReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            }
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ExceptionToJson(CHTTPReply::internal_server_error, E, LReply->Content);
                        AConnection->SendReply(CHTTPReply::internal_server_error);
                        Log()->Error(APP_LOG_EMERG, 0, E.what());
                    }
                }

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

                if (!Config()->PostgresConnect()) {

                    CString LResponse;

                    if (LPoint->Parse(ptJSON, LRequest, LResponse)) {
                        if (!LResponse.IsEmpty()) {
                            LWSReply->SetPayload(LResponse);
                            AConnection->SendWebSocket();
                        }
                    } else {
                        Log()->Error(APP_LOG_EMERG, 0, "Unknown WebSocket request: %s", LRequest.c_str());
                    }

                } else {

                    CJSONMessage jmRequest;
                    CJSONProtocol::Request(LRequest, jmRequest);

                    if (jmRequest.MessageTypeId == OCPP::mtCall) {
                        // Обработаем запрос в СУБД
                        DBParse(AConnection, LPoint->Identity(), jmRequest.Action, jmRequest.Payload);
                    } else {
                        // Ответ от зарядной станции отправим в обработчик
                        auto LHandler = LPoint->Messages()->FindMessageById(jmRequest.UniqueId);
                        if (Assigned(LHandler)) {
                            LHandler->Handler(AConnection);
                            delete LHandler;
                        }
                        LWSRequest->Clear();
                    }
                }
            } catch (std::exception &e) {
                //AConnection->SendWebSocketClose();
                AConnection->Clear();
                Log()->Error(APP_LOG_EMERG, 0, e.what());
            }
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