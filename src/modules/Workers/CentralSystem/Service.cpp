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

#include <openssl/sha.h>
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Workers {

        CString to_string(unsigned long Value) {
            TCHAR szString[_INT_T_LEN + 1] = {0};
            sprintf(szString, "%lu", Value);
            return CString(szString);
        }
        //--------------------------------------------------------------------------------------------------------------

        CString SHA1(const CString &data) {
            CString digest;
            digest.SetLength(SHA_DIGEST_LENGTH);
            ::SHA1((unsigned char *) data.data(), data.length(), (unsigned char *) digest.Data());
            return digest;
        }

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

        void CCSService::ReplyError(CHTTPServerConnection *AConnection, CReply::CStatusType ErrorCode, const CString &Message) {
            auto LReply = AConnection->Reply();

            if (ErrorCode == CReply::unauthorized) {
                CReply::AddUnauthorized(LReply, AConnection->Data()["Authorization"] != "Basic", "invalid_client", Message.c_str());
            }

            LReply->Content.Clear();
            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", ErrorCode, Delphi::Json::EncodeJsonString(Message).c_str());

            AConnection->SendReply(ErrorCode, nullptr, true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) {
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
                jmResponse.ErrorDescription = AException->what();

                CJSONProtocol::Response(jmResponse, LResponse);
#ifdef _DEBUG
                DebugMessage("\n[%p] [%s:%d] [%d] [WebSocket] Response:\n%s\n", LConnection, LConnection->Socket()->Binding()->PeerIP(),
                             LConnection->Socket()->Binding()->PeerPort(), LConnection->Socket()->Binding()->Handle(), LResponse.c_str());
#endif
                LWSReply->SetPayload(LResponse);
                LConnection->SendWebSocket(true);
            }

            Log()->Error(APP_LOG_EMERG, 0, AException->what());
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
            if (LConnection != nullptr && !LConnection->ClosedGracefully()) {
                try {
                    auto LPoint = CChargingPoint::FindOfConnection(LConnection);
                    Log()->Message(_T("[%s:%d] Point %s closed connection."),
                                   LConnection->Socket()->Binding()->PeerIP(),
                                   LConnection->Socket()->Binding()->PeerPort(),
                                   LPoint->Identity().IsEmpty() ? "(empty)" : LPoint->Identity().c_str());
                    delete LPoint;
                } catch (Delphi::Exception::Exception &E) {
                    Log()->Message(_T("[%s:%d] Point closed connection (%s)."),
                                   LConnection->Socket()->Binding()->PeerIP(),
                                   LConnection->Socket()->Binding()->PeerPort(), E.what());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CString CCSService::VerifyToken(const CString &Token) {

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
            const auto& ch = alg.substr(0, 2);

            const auto& Secret = GetSecret(Provider, Application);

            if (ch == "HS") {
                if (alg == "HS256") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{Secret});
                    verifier.verify(decoded);

                    return Token; // if algorithm HS256
                } else if (alg == "HS384") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs384{Secret});
                    verifier.verify(decoded);
                } else if (alg == "HS512") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs512{Secret});
                    verifier.verify(decoded);
                }
            } else if (ch == "RS") {

                const auto& kid = decoded.get_key_id();
                const auto& key = OAuth2::Helper::GetPublicKey(Providers, kid);

                if (alg == "RS256") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::rs256{key});
                    verifier.verify(decoded);
                } else if (alg == "RS384") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::rs384{key});
                    verifier.verify(decoded);
                } else if (alg == "RS512") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::rs512{key});
                    verifier.verify(decoded);
                }
            } else if (ch == "ES") {

                const auto& kid = decoded.get_key_id();
                const auto& key = OAuth2::Helper::GetPublicKey(Providers, kid);

                if (alg == "ES256") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::es256{key});
                    verifier.verify(decoded);
                } else if (alg == "ES384") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::es384{key});
                    verifier.verify(decoded);
                } else if (alg == "ES512") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::es512{key});
                    verifier.verify(decoded);
                }
            } else if (ch == "PS") {

                const auto& kid = decoded.get_key_id();
                const auto& key = OAuth2::Helper::GetPublicKey(Providers, kid);

                if (alg == "PS256") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::ps256{key});
                    verifier.verify(decoded);
                } else if (alg == "PS384") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::ps384{key});
                    verifier.verify(decoded);
                } else if (alg == "PS512") {
                    auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::ps512{key});
                    verifier.verify(decoded);
                }
            }

            const auto& Result = CCleanToken(R"({"alg":"HS256","typ":"JWT"})", decoded.get_payload(), true);

            return Result.Sign(jwt::algorithm::hs256{Secret});
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckAuthorizationData(CRequest *ARequest, CAuthorization &Authorization) {

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
            auto LReply = AConnection->Reply();

            try {
                if (CheckAuthorizationData(LRequest, Authorization)) {
                    if (Authorization.Schema == CAuthorization::asBearer) {
                        Authorization.Token = VerifyToken(Authorization.Token);
                        return true;
                    }
                }

                if (Authorization.Schema == CAuthorization::asBasic)
                    AConnection->Data().Values("Authorization", "Basic");

                ReplyError(AConnection, CReply::unauthorized, "Unauthorized.");
            } catch (jwt::token_expired_exception &e) {
                ReplyError(AConnection, CReply::forbidden, e.what());
            } catch (jwt::token_verification_exception &e) {
                ReplyError(AConnection, CReply::bad_request, e.what());
            } catch (CAuthorizationError &e) {
                ReplyError(AConnection, CReply::bad_request, e.what());
            } catch (std::exception &e) {
                ReplyError(AConnection, CReply::bad_request, e.what());
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoOCPP(CHTTPServerConnection *AConnection) {
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            LReply->ContentType = CReply::html;

            CStringList LPath;
            SplitColumns(LRequest->Location.pathname, LPath, '/');

            if (LPath.Count() < 2) {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            const CString& LSecWebSocketKey = LRequest->Headers.Values("sec-websocket-key");
            if (LSecWebSocketKey.IsEmpty()) {
                AConnection->SendStockReply(CReply::bad_request);
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
                LPoint->Address() = GetHost(AConnection);
            } else {
                LPoint->SwitchConnection(AConnection);
                LPoint->Address() = GetHost(AConnection);
            }
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
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            const auto &LService = LPath[0];

            if (LPath.Count() < 3) {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            const auto &LVersion = LPath[1];
            const auto &LCommand = LPath[2];

            try {

                if (LVersion == "v1") {

                    LReply->ContentType = CReply::json;

                    if (LCommand == "ping") {

                        AConnection->SendStockReply(CReply::ok);
                        return;

                    } else if (LCommand == "time") {

                        LReply->Content << "{\"serverTime\": " << to_string(MsEpoch()) << "}";
                        AConnection->SendReply(CReply::ok);
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

                        AConnection->SendReply(CReply::ok);

                        return;

                    } else if (LCommand == "ChargePoint") {

                        CAuthorization Authorization;
                        if (!CheckAuthorization(AConnection, Authorization)) {
                            return;
                        }

                        if (LPath.Count() < 5) {
                            AConnection->SendStockReply(CReply::not_found);
                            return;
                        }

                        const auto &LIdentity = LPath[3];
                        const auto &LAction = LPath[4];

                        auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);

                        if (LPoint == nullptr) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 300,
                                                   "Charge point not found.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        auto LConnection = LPoint->Connection();
                        if (LConnection == nullptr) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 301,
                                                   "Charge point offline.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        if (!LConnection->Connected()) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 302,
                                                   "Charge point not connected.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        if (LConnection->Protocol() != pWebSocket) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 303,
                                                   "Incorrect charge point protocol version.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        auto OnRequest = [AConnection](OCPP::CMessageHandler *AHandler,
                                CHTTPServerConnection *AWSConnection) {

                            auto LReply = AConnection->Reply();

                            auto LWSRequest = AWSConnection->WSRequest();
                            const CString LRequest(LWSRequest->Payload());

                            CReply::CStatusType Status = CReply::ok;

                            try {
                                CJSONMessage LMessage;

                                if (!CJSONProtocol::Request(LRequest, LMessage)) {
                                    Status = CReply::bad_request;
                                }

                                LReply->Content = LMessage.Payload.ToString();
                                AConnection->SendReply(Status, nullptr, true);
                            } catch (std::exception &e) {
                                ReplyError(AConnection, CReply::internal_server_error, e.what());
                                Log()->Error(APP_LOG_EMERG, 0, e.what());
                            }

                            AConnection->CloseConnection(true);

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

                AConnection->SendStockReply(CReply::not_found);
            } catch (std::exception &e) {
                ReplyError(AConnection, CReply::internal_server_error, e.what());
                Log()->Error(APP_LOG_EMERG, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();

            CString LPath(LRequest->Location.pathname);

            // Request path must be absolute and not contain "..".
            if (LPath.empty() || LPath.front() != '/' || LPath.find(_T("..")) != CString::npos) {
                AConnection->SendStockReply(CReply::bad_request);
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
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            const auto& LService = LPath[0];

            if (LService == "api") {

                if (LPath.Count() < 3) {
                    AConnection->SendStockReply(CReply::not_found);
                    return;
                }

                const auto& LVersion = LPath[1];
                const auto& LCommand = LPath[2];

                if (LVersion == "v1") {

                    LReply->ContentType = CReply::json;

                    try {
                        CAuthorization Authorization;
                        if (!CheckAuthorization(AConnection, Authorization)) {
                            return;
                        }

                        if (LCommand == "ChargePoint") {

                            if (LPath.Count() < 5) {
                                AConnection->SendStockReply(CReply::not_found);
                                return;
                            }

                            const auto &LIdentity = LPath[3];
                            const auto &LAction = LPath[4];

                            auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);

                            if (LPoint == nullptr) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 300,
                                                       "Charge point not found.");
                                AConnection->SendReply(CReply::ok);
                                return;
                            }

                            auto LConnection = LPoint->Connection();
                            if (LConnection == nullptr) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 301,
                                                       "Charge point offline.");
                                AConnection->SendReply(CReply::ok);
                                return;
                            }

                            if (!LConnection->Connected()) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 302,
                                                       "Charge point not connected.");
                                AConnection->SendReply(CReply::ok);
                                return;
                            }

                            if (LConnection->Protocol() != pWebSocket) {
                                LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 303,
                                                       "Incorrect charge point protocol version.");
                                AConnection->SendReply(CReply::ok);
                                return;
                            }

                            auto OnRequest = [AConnection](OCPP::CMessageHandler *AHandler,
                                                           CHTTPServerConnection *AWSConnection) {

                                auto LReply = AConnection->Reply();

                                auto LWSRequest = AWSConnection->WSRequest();
                                const CString LRequest(LWSRequest->Payload());

                                CReply::CStatusType Status = CReply::ok;

                                try {
                                    CJSONMessage LMessage;

                                    if (!CJSONProtocol::Request(LRequest, LMessage)) {
                                        Status = CReply::bad_request;
                                    }

                                    LReply->Content = LMessage.Payload.ToString();
                                    AConnection->SendReply(Status, nullptr, true);
                                } catch (std::exception &e) {
                                    ReplyError(AConnection, CReply::internal_server_error, e.what());
                                    Log()->Error(APP_LOG_EMERG, 0, e.what());
                                }

                                AWSConnection->ConnectionStatus(csReplySent);
                            };

                            CJSON LPayload;

                            ContentToJson(LRequest, LPayload);

                            if (LAction == "ChangeConfiguration") {

                                const auto &key = LPayload["key"].AsString();
                                const auto &value = LPayload["value"].AsString();

                                if (key.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                if (value.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "RemoteStartTransaction") {

                                const auto &connectorId = LPayload["connectorId"].AsString();
                                const auto &idTag = LPayload["idTag"].AsString();

                                if (connectorId.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                if (idTag.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "RemoteStopTransaction") {

                                const auto &transactionId = LPayload["transactionId"].AsString();

                                if (transactionId.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
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
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                if (expiryDate.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                if (idTag.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                if (reservationId.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "CancelReservation") {

                                const auto &reservationId = LPayload["reservationId"].AsString();

                                if (reservationId.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            } else if (LAction == "Reset") {

                                const auto &type = LPayload["type"].AsString();

                                if (type.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            } else if (LAction == "TriggerMessage") {

                                const auto &requestedMessage = LPayload["requestedMessage"].AsString();

                                if (requestedMessage.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            } else if (LAction == "UnlockConnector") {

                                const auto &connectorId = LPayload["connectorId"].AsString();

                                if (connectorId.IsEmpty()) {
                                    AConnection->SendStockReply(CReply::bad_request);
                                    return;
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            }
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ExceptionToJson(CReply::internal_server_error, E, LReply->Content);
                        AConnection->SendReply(CReply::internal_server_error);
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

                AConnection->SendReply(CReply::ok, "application/soap+xml");

                return;
            }

            AConnection->SendStockReply(CReply::not_found);
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
                        }
                    }
                }
            } catch (std::exception &e) {
                //AConnection->SendWebSocketClose();
                //AConnection->CloseConnection(true);
                Log()->Error(APP_LOG_EMERG, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Heartbeat() {
            CApostolModule::Heartbeat();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Execute(CHTTPServerConnection *AConnection) {
            switch (AConnection->Protocol()) {
                case pHTTP:
                    CApostolModule::Execute(AConnection);
                    break;
                case pWebSocket:
#ifdef _DEBUG
                    WSDebugConnection(AConnection);
#endif
                    DoWebSocket(AConnection);
                    break;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::IsEnabled() {
            if (m_ModuleStatus == msUnknown)
                m_ModuleStatus = msEnabled;
            return m_ModuleStatus == msEnabled;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckUserAgent(const CString &Value) {
            return IsEnabled();
        }

    }
}
}