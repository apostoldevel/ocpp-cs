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
            DebugMessage("\n[%p] Request:\n%s %s HTTP/%d.%d\n", ARequest, ARequest->Method.c_str(), ARequest->Uri.c_str(), ARequest->VMajor, ARequest->VMinor);

            for (int i = 0; i < ARequest->Headers.Count(); i++)
                DebugMessage("%s: %s\n", ARequest->Headers[i].Name.c_str(), ARequest->Headers[i].Value.c_str());

            if (!ARequest->Content.IsEmpty())
                DebugMessage("\n%s\n", ARequest->Content.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DebugReply(CReply *AReply) {
            TCHAR ch;
            CString String;
            CMemoryStream Stream;

            AReply->ToBuffers(&Stream);
            Stream.Position(0);

            for (size_t i = 0; i < Stream.Size(); ++i) {
                Stream.Read(&ch, 1);
                if (ch != '\r')
                    String.Append(ch);
            }

            DebugMessage("\n[%p] Reply:\n%s\n", AReply, String.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::ExceptionToJson(int ErrorCode, const std::exception &AException, CString& Json) {
            TCHAR ch;
            LPCTSTR lpMessage = AException.what();
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

                ExceptionToJson(0, *AException, LReply->Content);

                LConnection->SendReply(LStatus);
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
                            throw Delphi::Exception::EDBError(Response["error"].AsSiring().c_str());

                        jmResponse.Payload << Response.ToString();
                    }
                } catch (Delphi::Exception::Exception &E) {
                    jmResponse.MessageTypeId = mtCallError;
                    jmResponse.ErrorCode = "InternalError";
                    jmResponse.ErrorDescription = E.what();

                    Log()->Error(APP_LOG_EMERG, 0, E.what());
                }

                CJSONProtocol::Response(jmResponse, LResponse);

                DebugMessage("WS Response:\n%s\n", LResponse.c_str());

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

            if (LQuery->QueryStart() != POLL_QUERY_START_ERROR) {
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

        void CCSService::DoWWW(CHTTPServerConnection *AConnection) {
            auto LServer = dynamic_cast<CHTTPServer *> (AConnection->Server());
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            TCHAR szExt[PATH_MAX] = {0};

            LReply->ContentType = CReply::html;

            // Decode url to path.
            CString LRequestPath;
            if (!LServer->URLDecode(LRequest->Uri, LRequestPath)) {
                AConnection->SendStockReply(CReply::bad_request);
                return;
            }

            // Request path must be absolute and not contain "..".
            if (LRequestPath.empty() || LRequestPath.front() != '/' || LRequestPath.find("..") != CString::npos) {
                AConnection->SendStockReply(CReply::bad_request);
                return;
            }

            // If path ends in slash (i.e. is a directory) then add "index.html".
            if (LRequestPath.back() == '/') {
                LRequestPath += "index.html";
            }

            // Open the file to send back.
            const CString LFullPath = LServer->DocRoot() + LRequestPath;
            if (!FileExists(LFullPath.c_str())) {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            LReply->Content.LoadFromFile(LFullPath.c_str());

            // Fill out the CReply to be sent to the client.
            AConnection->SendReply(CReply::ok, Mapping::ExtToType(ExtractFileExt(szExt, LRequestPath.c_str())));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CStringList LUri;
            SplitColumns(LRequest->Uri.c_str(), LRequest->Uri.Size(), &LUri, '/');

            if (LUri.Count() == 0) {
                DoWWW(AConnection);
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
                DoWWW(AConnection);
                return;
            }

            AConnection->SendStockReply(CReply::not_found);
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

                    if (LCommand == "ChargePoint") {

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

                        if (LAction == "ChangeConfiguration") {

                            const CString& LKey = LRequest->Params["key"];
                            const CString& LValue = LRequest->Params["value"];

                            if (LKey.IsEmpty()) {
                                AConnection->SendStockReply(CReply::bad_request);
                                return;
                            }

                            if (LValue.IsEmpty()) {
                                AConnection->SendStockReply(CReply::bad_request);
                                return;
                            }

                            CJSON LPayload(jvtObject);

                            LPayload.Object().AddPair("key", LKey);
                            LPayload.Object().AddPair("value", LValue);

                            LPoint->Messages()->Add(OnRequest, LAction, LPayload);

                            return;
                        }
                    }
                }

            } else if (LService == "Ocpp") {

                auto LPoint = m_CPManager->FindPointByConnection(AConnection);
                if (LPoint == nullptr) {
                    LPoint = m_CPManager->Add(AConnection);
                    AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
                }

                LPoint->Parse(ptSOAP, LRequest->Content, LReply->Content);

                AConnection->SendReply(CReply::ok, "application/soap+xml");

                return;
            }

            AConnection->SendStockReply(CReply::not_found);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoHTTP(CHTTPServerConnection *AConnection) {
            int i = 0;
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();
#ifdef _DEBUG
            DebugMessage("[%p][%s:%d][%d]", AConnection, AConnection->Socket()->Binding()->PeerIP(),
                         AConnection->Socket()->Binding()->PeerPort(), AConnection->Socket()->Binding()->Handle());

            DebugRequest(AConnection->Request());

            static auto OnReply = [](CObject *Sender) {
                auto LConnection = dynamic_cast<CHTTPServerConnection *> (Sender);
                DebugReply(LConnection->Reply());
            };

            AConnection->OnReply(OnReply);
#endif
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

        void CCSService::DoWebSocket(CHTTPServerConnection *AConnection) {

            auto LWSRequest = AConnection->WSRequest();
            auto LWSReply = AConnection->WSReply();

            const CString LRequest(LWSRequest->Payload());

            DebugMessage("[%p][%s:%d][%d] WS Request:\n%s\n", AConnection, AConnection->Socket()->Binding()->PeerIP(),
                         AConnection->Socket()->Binding()->PeerPort(), AConnection->Socket()->Binding()->Handle(), LRequest.c_str());

            try {
                auto LPoint = m_CPManager->FindPointByConnection(AConnection);
                if (LPoint == nullptr)
                    throw Delphi::Exception::Exception("Not found charge point by connection.");

                if (!Config()->PostgresConnect()) {

                    CString LResponse;

                    if (LPoint->Parse(ptJSON, LRequest, LResponse)) {
                        if (!LResponse.IsEmpty()) {
                            DebugMessage("WS Response:\n%s\n", LResponse.c_str());
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

        void CCSService::Execute(CHTTPServerConnection *AConnection) {
            switch (AConnection->Protocol()) {
                case pHTTP:
                    DoHTTP(AConnection);
                    break;
                case pWebSocket:
                    DoWebSocket(AConnection);
                    break;
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