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

extern "C++" {

namespace Apostol {

    namespace CSService {

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

        CCSService::CCSService(CModuleManager *AManager) : CApostolModule(AManager) {
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
            m_pMethods->AddObject(_T("GET"), (CObject *) new CMethodHandler(true, std::bind(&CWebService::DoGet, this, _1)));
            m_pMethods->AddObject(_T("POST"), (CObject *) new CMethodHandler(true, std::bind(&CWebService::DoPost, this, _1)));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true, std::bind(&CWebService::DoOptions, this, _1)));
            m_pMethods->AddObject(_T("HEAD"), (CObject *) new CMethodHandler(true, std::bind(&CWebService::DoHead, this, _1)));
            m_pMethods->AddObject(_T("PUT"), (CObject *) new CMethodHandler(false, std::bind(&CWebService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("DELETE"), (CObject *) new CMethodHandler(false, std::bind(&CWebService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("TRACE"), (CObject *) new CMethodHandler(false, std::bind(&CWebService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PATCH"), (CObject *) new CMethodHandler(false, std::bind(&CWebService::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CWebService::MethodNotAllowed, this, _1)));
#endif
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

                jmResponse.MessageTypeId = mtCallError;
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
                    jmResponse.MessageTypeId = mtCallError;
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
                auto LPoint = m_CPManager->FindPointByConnection(LConnection);
                if (LPoint != nullptr) {
                    Log()->Message(_T("[%s:%d] Point %s closed connection."), LConnection->Socket()->Binding()->PeerIP(),
                                   LConnection->Socket()->Binding()->PeerPort(),
                                   LPoint->Identity().IsEmpty() ? "(empty)" : LPoint->Identity().c_str());
                    delete LPoint;
                } else {
                    Log()->Message(_T("[%s:%d] Point closed connection."), LConnection->Socket()->Binding()->PeerIP(),
                                   LConnection->Socket()->Binding()->PeerPort());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization) {
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            const auto& LAuthorization = LRequest->Headers.Values(_T("Authorization"));
            if (LAuthorization.IsEmpty())
                return false;

            try {
                Authorization << LAuthorization;
                if (Authorization.Username == "ocpp" && Authorization.Password == Config()->APIPassphrase()) {
                    return true;
                }
            } catch (std::exception &e) {
                Log()->Error(APP_LOG_EMERG, 0, e.what());
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoAPI(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CStringList LUri;
            SplitColumns(LRequest->Location.pathname, LUri, '/');

            if (LUri.Count() == 0) {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            const auto& LService = LUri[0];

            if (LService == "api") {

                if (LUri.Count() < 3) {
                    AConnection->SendStockReply(CReply::not_found);
                    return;
                }

                const auto& LVersion = LUri[1];
                const auto& LCommand = LUri[2];

                if (LVersion == "v1") {

                    LReply->ContentType = CReply::json;

                    if (LCommand == "ping") {

                        AConnection->SendStockReply(CReply::ok);
                        return;

                    } else if (LCommand == "time") {

                        LReply->Content << "{\"serverTime\": " << to_string(MsEpoch()) << "}";
                        AConnection->SendReply(CReply::ok);
                        return;

                    } else  if (LCommand == "ChargePointList") {

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

                        AConnection->SendReply(CReply::ok);

                        return;

                    } else if (LCommand == "ChargePoint") {

                        if (LUri.Count() < 5) {
                            AConnection->SendStockReply(CReply::not_found);
                            return;
                        }

                        const auto& LIdentity = LUri[3];
                        const auto& LAction = LUri[4];

                        auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);

                        if (LPoint == nullptr) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 300, "Charge point not found.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        auto LConnection = LPoint->Connection();
                        if (LConnection == nullptr) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 301, "Charge point offline.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        if (!LConnection->Connected()) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 302, "Charge point not connected.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        if (LConnection->Protocol() != pWebSocket) {
                            LReply->Content.Format(R"({"error": {"code": %u, "message": "%s"}})", 303, "Incorrect charge point protocol version.");
                            AConnection->SendReply(CReply::ok);
                            return;
                        }

                        auto OnRequest = [AConnection](CMessageHandler *AHandler, CHTTPServerConnection *AWSConnection) {
                            auto LWSRequest = AWSConnection->WSRequest();
                            const CString LRequest(LWSRequest->Payload());

                            CJSONMessage LMessage;
                            CJSONProtocol::Request(LRequest, LMessage);

                            try {
                                auto LReply = AConnection->Reply();
                                LReply->Content = LMessage.Payload.ToString();
                                AConnection->SendReply(CReply::ok, nullptr, true);
                            } catch (std::exception &e) {
                                Log()->Error(APP_LOG_EMERG, 0, e.what());
                            }

                            AWSConnection->ConnectionStatus(csReplySent);
                        };

                        if (LAction == "GetConfiguration") {

                            CJSON LPayload(jvtObject);
                            CJSONValue LArray(jvtArray);

                            const CString& LKey = LRequest->Params["key"];
                            if (!LKey.IsEmpty()) {
                                LArray.Array().Add(LKey);
                                LPayload.Object().AddPair("key", LArray);
                            }

                            LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                            return;
                        }
                    }
                }

            } else if (LService == "ocpp") {

                LReply->ContentType = CReply::html;

                if (LUri.Count() < 2) {
                    AConnection->SendStockReply(CReply::not_found);
                    return;
                }

                const CString& LSecWebSocketKey = LRequest->Headers.Values("sec-websocket-key");
                if (LSecWebSocketKey.IsEmpty()) {
                    AConnection->SendStockReply(CReply::bad_request);
                    return;
                }

                const auto& LIdentity = LUri[1];
                const CString LAccept(SHA1(LSecWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
                const auto& LSecWebSocketProtocol = LRequest->Headers.Values("sec-websocket-protocol");
                const CString LProtocol(LSecWebSocketProtocol.IsEmpty() ? "" : LSecWebSocketProtocol.SubString(0, LSecWebSocketProtocol.Find(',')));

                AConnection->SwitchingProtocols(LAccept, LProtocol);

                auto LPoint = m_CPManager->FindPointByIdentity(LIdentity);
                if (LPoint == nullptr) {
                    LPoint = m_CPManager->Add(AConnection);
                    LPoint->Identity() = LIdentity;
                    LPoint->Address() = LRequest->Headers.Values("x-real-ip");
                    AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
                } else {
                    try {
                        LPoint->SwitchConnection(AConnection);
                    } catch (std::exception &e) {
                        Log()->Error(APP_LOG_EMERG, 0, e.what());
                        delete LPoint;
                    }
                }

                return;
            } else {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            AConnection->SendStockReply(CReply::not_found);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {
            auto LServer = dynamic_cast<CHTTPServer *> (AConnection->Server());
            auto LRequest = AConnection->Request();

            // Decode url to path.
            CString url;
            if (!CHTTPServer::URLDecode(LRequest->URI, url)) {
                AConnection->SendStockReply(CReply::bad_request);
                return;
            }

            CLocation Location(url);
            auto& requestPath = Location.pathname;

            // Request path must be absolute and not contain "..".
            if (requestPath.empty() || requestPath.front() != '/' || requestPath.find("..") != CString::npos) {
                AConnection->SendStockReply(CReply::bad_request);
                return;
            }

            CAuthorization Authorization;
            if (!CheckAuthorization(AConnection, Authorization)) {
                AConnection->SendStockReply(CReply::unauthorized);
                return;
            }

            if (requestPath.SubString(0, 5) == "/api/") {
                DoAPI(AConnection);
                return;
            }

            SendResource(AConnection, requestPath);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoPost(CHTTPServerConnection *AConnection) {
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CStringList LUri;
            SplitColumns(LRequest->Location.pathname, LUri, '/');

            if (LUri.Count() < 2) {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            const auto& LService = LUri[0];

            if (LService == "api") {

                if (LUri.Count() < 3) {
                    AConnection->SendStockReply(CReply::not_found);
                    return;
                }

                const auto& LVersion = LUri[1];
                const auto& LCommand = LUri[2];

                if (LVersion == "v1") {

                    LReply->ContentType = CReply::json;

                    const CString &LContentType = LRequest->Headers.Values("content-type");
                    if (!LContentType.IsEmpty() && LRequest->ContentLength == 0) {
                        AConnection->SendStockReply(CReply::no_content);
                        return;
                    }

                    try {
                        CAuthorization Authorization;
                        if (!CheckAuthorization(AConnection, Authorization)) {
                            AConnection->SendStockReply(CReply::unauthorized);
                            return;
                        }

                        if (LCommand == "ChargePoint") {

                            if (LUri.Count() < 5) {
                                AConnection->SendStockReply(CReply::not_found);
                                return;
                            }

                            const auto &LIdentity = LUri[3];
                            const auto &LAction = LUri[4];

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

                            auto OnRequest = [AConnection](CMessageHandler *AHandler,
                                                           CHTTPServerConnection *AWSConnection) {
                                auto LWSRequest = AWSConnection->WSRequest();
                                const CString LRequest(LWSRequest->Payload());

                                CJSONMessage LMessage;
                                CJSONProtocol::Request(LRequest, LMessage);

                                try {
                                    auto LReply = AConnection->Reply();
                                    LReply->Content = LMessage.Payload.ToString();
                                    AConnection->SendReply(CReply::ok, nullptr, true);
                                } catch (std::exception &e) {
                                    Log()->Error(APP_LOG_EMERG, 0, e.what());
                                }

                                AWSConnection->ConnectionStatus(csReplySent);
                            };

                            if (LAction == "ChangeConfiguration") {

                                CJSON LPayload(jvtObject);

                                if (LContentType == "application/json") {

                                    LPayload << LRequest->Content;

                                } else {
                                    const CString &key = LRequest->Params["key"];
                                    const CString &value = LRequest->Params["value"];

                                    if (key.IsEmpty()) {
                                        AConnection->SendStockReply(CReply::bad_request);
                                        return;
                                    }

                                    if (value.IsEmpty()) {
                                        AConnection->SendStockReply(CReply::bad_request);
                                        return;
                                    }

                                    LPayload.Object().AddPair("key", key);
                                    LPayload.Object().AddPair("value", value);
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "RemoteStartTransaction") {

                                CJSON LPayload(jvtObject);

                                if (LContentType == "application/json") {

                                    LPayload << LRequest->Content;

                                } else {
                                    const CString &connectorId = LRequest->Params["connectorId"];
                                    const CString &idTag = LRequest->Params["idTag"];

                                    if (connectorId.IsEmpty()) {
                                        AConnection->SendStockReply(CReply::bad_request);
                                        return;
                                    }

                                    if (idTag.IsEmpty()) {
                                        AConnection->SendStockReply(CReply::bad_request);
                                        return;
                                    }

                                    LPayload.Object().AddPair("connectorId", connectorId);
                                    LPayload.Object().AddPair("idTag", idTag);
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "RemoteStopTransaction") {

                                CJSON LPayload(jvtObject);

                                if (LContentType == "application/json") {

                                    LPayload << LRequest->Content;

                                } else {
                                    const CString &transactionId = LRequest->Params["transactionId"];

                                    if (transactionId.IsEmpty()) {
                                        AConnection->SendStockReply(CReply::bad_request);
                                        return;
                                    }

                                    LPayload.Object().AddPair("transactionId", transactionId);
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "ReserveNow") {

                                CJSON LPayload(jvtObject);

                                if (LContentType == "application/json") {

                                    LPayload << LRequest->Content;

                                } else {
                                    const CString &connectorId = LRequest->Params["connectorId"];
                                    const CString &expiryDate = LRequest->Params["expiryDate"];
                                    const CString &idTag = LRequest->Params["idTag"];
                                    const CString &parentIdTag = LRequest->Params["parentIdTag"];
                                    const CString &reservationId = LRequest->Params["reservationId"];

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

                                    LPayload.Object().AddPair("connectorId", connectorId);
                                    LPayload.Object().AddPair("expiryDate", expiryDate);
                                    LPayload.Object().AddPair("idTag", idTag);

                                    if (!parentIdTag.IsEmpty())
                                        LPayload.Object().AddPair("parentIdTag", parentIdTag);

                                    LPayload.Object().AddPair("reservationId", reservationId);
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;

                            } else if (LAction == "CancelReservation") {

                                CJSON LPayload(jvtObject);

                                if (LContentType == "application/json") {

                                    LPayload << LRequest->Content;

                                } else {
                                    const CString &reservationId = LRequest->Params["reservationId"];

                                    if (reservationId.IsEmpty()) {
                                        AConnection->SendStockReply(CReply::bad_request);
                                        return;
                                    }

                                    LPayload.Object().AddPair("reservationId", reservationId);
                                }

                                LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                                return;
                            }
                        }
                    } catch (Delphi::Exception::Exception &E) {
                        ExceptionToJson(0, E, LReply->Content);
                        AConnection->SendReply(CReply::bad_request);
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
#ifdef _DEBUG
            DebugMessage("\n[%p] [%s:%d] [%d] [WebSocket] Request:\n%s\n", AConnection, AConnection->Socket()->Binding()->PeerIP(),
                         AConnection->Socket()->Binding()->PeerPort(), AConnection->Socket()->Binding()->Handle(), LRequest.c_str());
#endif
            try {
                auto LPoint = m_CPManager->FindPointByConnection(AConnection);
                if (LPoint == nullptr)
                    throw Delphi::Exception::Exception("Not found charge point by connection.");

                if (!Config()->PostgresConnect()) {

                    CString LResponse;

                    if (LPoint->Parse(ptJSON, LRequest, LResponse)) {
                        if (!LResponse.IsEmpty()) {
#ifdef _DEBUG
                            DebugMessage("\n[%p] [%s:%d] [%d] [WebSocket] Response:\n%s\n", AConnection, AConnection->Socket()->Binding()->PeerIP(),
                                         AConnection->Socket()->Binding()->PeerPort(), AConnection->Socket()->Binding()->Handle(), LResponse.c_str());
#endif
                            LWSReply->SetPayload(LResponse);
                            AConnection->SendWebSocket();
                        }
                    } else {
                        Log()->Error(APP_LOG_EMERG, 0, "Unknown WebSocket request: %s", LRequest.c_str());
                    }

                } else {

                    CJSONMessage jmRequest;
                    CJSONProtocol::Request(LRequest, jmRequest);

                    if (jmRequest.MessageTypeId == mtCall) {
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
                AConnection->SendWebSocketClose();
                AConnection->CloseConnection(true);
                Log()->Error(APP_LOG_EMERG, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Heartbeat() {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::Execute(CHTTPServerConnection *AConnection) {
            switch (AConnection->Protocol()) {
                case pHTTP:
                    CApostolModule::Execute(AConnection);
                    break;
                case pWebSocket:
                    DoWebSocket(AConnection);
                    break;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCSService::CheckUserAgent(const CString& Value) {
            return true;
        }

    }
}
}