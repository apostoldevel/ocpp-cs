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
                    Log()->Message(_T("[%s:%d] Point closed connection."), LConnection->Socket()->Binding()->PeerIP(),
                                   LConnection->Socket()->Binding()->PeerPort());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoGet(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            CStringList LUri;
            SplitColumns(LRequest->Uri.c_str(), LRequest->Uri.Size(), &LUri, '/');

            if (LUri.Count() < 2) {
                AConnection->SendStockReply(CReply::not_found);
                return;
            }

            const auto& LService = LUri[0].Lower();

            if (LService == "api") {

                if (LUri.Count() < 3) {
                    AConnection->SendStockReply(CReply::not_found);
                    return;
                }

                const auto& LVersion = LUri[1].Lower();
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

                        if (LAction == "GetConfiguration") {

                            auto OnRequest = [AConnection](CObject *Sender) {
                                auto LWSConnection = dynamic_cast<CHTTPServerConnection *> (Sender);

                                auto LWSRequest = LWSConnection->WSRequest();
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

                                LWSConnection->ConnectionStatus(csReplySent);
                                LWSConnection->OnRequest(nullptr);
                            };

                            CString Result;
                            const auto& UniqueId = GetUID(APOSTOL_MODULE_UID_LENGTH);

                            CJSON LPayload(jvtObject);
                            CJSONValue LArray(jvtArray);

                            const CString& LKey = LRequest->Params["key"];
                            if (!LKey.IsEmpty()) {
                                LArray.Array().Add(LKey);
                                LPayload.Object().AddPair("key", LArray);
                            }

                            CJSONProtocol::Call(UniqueId, LAction, LPayload, Result);

                            auto LWSReply = LConnection->WSReply();

                            LWSReply->Clear();
                            LWSReply->SetPayload(Result);

                            LConnection->OnRequest(OnRequest);
                            LConnection->SendWebSocket(true);

                            return;
                        }
                    }
                }

            } else if (LService == "ocpp") {

                LReply->ContentType = CReply::html;

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
                    AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
                } else {
                    LPoint->SwitchConnection(AConnection);
                }

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

            auto LPoint = m_CPManager->FindPointByConnection(AConnection);
            if (LPoint == nullptr) {
                LPoint = m_CPManager->Add(AConnection);
                AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
            }

            LPoint->Parse(ptSOAP, LRequest->Content, LReply->Content);

            AConnection->SendReply(CReply::ok, "application/soap+xml");
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCSService::DoHTTP(CHTTPServerConnection *AConnection) {
            int i = 0;
            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            DebugRequest(AConnection->Request());

            static auto DoReply = [](CObject *Sender) {
                auto LConnection = dynamic_cast<CHTTPServerConnection *> (Sender);
                DebugReply(LConnection->Reply());
            };

            AConnection->OnReply(DoReply);

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

            const CString Request(LWSRequest->Payload());
            CString Response;

            try {
                auto LPoint = m_CPManager->FindPointByConnection(AConnection);
                if (LPoint == nullptr) {
                    LPoint = m_CPManager->Add(AConnection);
                    AConnection->OnDisconnected(std::bind(&CCSService::DoPointDisconnected, this, _1));
                }

                DebugMessage("WS Request:\n%s\n", Request.c_str());

                if (LPoint->Parse(ptJSON, Request, Response)) {
                    DebugMessage("WS Response:\n%s\n", Response.c_str());
                    LWSReply->SetPayload(Response);
                    AConnection->SendWebSocket();
                }
            } catch (std::exception &e) {
                AConnection->CloseConnection(true);
                AConnection->SendWebSocketClose();
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