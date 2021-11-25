/*++

Program name:

  OCPP Charge Point Service

Module Name:

  CPModule.cpp

Notices:

  Module: Charge Point Web Server

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

//----------------------------------------------------------------------------------------------------------------------

#include "Core.hpp"
#include "CPModule.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include "jwt.h"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace CPModule {

        CString to_string(unsigned long Value) {
            TCHAR szString[_INT_T_LEN + 1] = {0};
            sprintf(szString, "%lu", Value);
            return {szString};
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CModuleConnection -----------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CModuleConnection::HostToURL(const CString &Host) {
            if (!Host.IsEmpty()) {
                int Port = 0;

                size_t Pos = Host.Find(':');

                if (Pos != CString::npos) {
                    Port = StrToInt(Host.SubString(Pos + 1).c_str());
                }

                if (Port == 443) {
                    m_URL = "https://" + Host;
                } else {
                    m_URL = "http://" + Host;
                }
            }
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CCPModule -------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCPModule::CCPModule(CModuleProcess *AProcess) : CApostolModule(AProcess, "charge point", "module/ChargePoint") {
            m_Headers.Add("Authorization");

            CCPModule::InitMethods();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPModule::InitMethods() {
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
            m_pMethods->AddObject(_T("GET"), (CObject *) new CMethodHandler(true, std::bind(&CCPModule::DoGet, this, _1)));
            m_pMethods->AddObject(_T("POST"), (CObject *) new CMethodHandler(true, std::bind(&CCPModule::DoPost, this, _1)));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true, std::bind(&CCPModule::DoOptions, this, _1)));
            m_pMethods->AddObject(_T("HEAD"), (CObject *) new CMethodHandler(true, std::bind(&CCPModule::DoHead, this, _1)));
            m_pMethods->AddObject(_T("PUT"), (CObject *) new CMethodHandler(false, std::bind(&CCPModule::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("DELETE"), (CObject *) new CMethodHandler(false, std::bind(&CCPModule::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("TRACE"), (CObject *) new CMethodHandler(false, std::bind(&CCPModule::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PATCH"), (CObject *) new CMethodHandler(false, std::bind(&CCPModule::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CCPModule::MethodNotAllowed, this, _1)));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPModule::DoGet(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            pReply->ContentType = CHTTPReply::json;

            CStringList slRouts;
            SplitColumns(pRequest->Location.pathname, slRouts, '/');

            if (slRouts.Count() < 3) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& caService = slRouts[0].Lower();
            const auto& caVersion = slRouts[1].Lower();
            const auto& caCommand = slRouts[2].Lower();
            const auto& caAction = slRouts.Count() == 4 ? slRouts[3].Lower() : "";

            if (caService != "api" || (caVersion != "v1")) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            CHTTPReply::CStatusType status = CHTTPReply::bad_request;

            AConnection->Data().Values("path", pRequest->Location.pathname.Lower());

            try {
                if (caCommand == "ping") {

                    AConnection->SendStockReply(CHTTPReply::ok);

                } else if (caCommand == "time") {

                    pReply->Content << "{\"serverTime\": " << to_string(MsEpoch()) << "}";

                    AConnection->SendReply(CHTTPReply::ok);

                } else {

                    CAuthorization Authorization;
                    if (!CheckAuthorization(AConnection, Authorization)) {
                        return;
                    }

                    AConnection->SendStockReply(CHTTPReply::not_found);
                }
            } catch (Delphi::Exception::Exception &E) {
                ExceptionToJson(status, E, pReply->Content);

                AConnection->SendReply(CHTTPReply::ok);
                Log()->Error(APP_LOG_EMERG, 0, E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPModule::DoPost(CHTTPServerConnection *AConnection) {

            auto pRequest = AConnection->Request();
            auto pReply = AConnection->Reply();

            pReply->ContentType = CHTTPReply::json;

            CStringList slRouts;
            SplitColumns(pRequest->Location.pathname, slRouts, '/');

            if (slRouts.Count() < 3) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& caService = slRouts[0].Lower();
            const auto& caVersion = slRouts[1].Lower();
            const auto& caCommand = slRouts[2].Lower();

            const auto& caAction = slRouts.Count() == 4 ? slRouts[3].Lower() : "";

            if (caService != "api" || caVersion != "v1") {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            const auto& caContentType = pRequest->Headers.Values("content-type");
            if (!caContentType.IsEmpty() && pRequest->ContentLength == 0) {
                AConnection->SendStockReply(CHTTPReply::no_content);
                return;
            }

            try {
                const auto& caHost = pRequest->Headers["host"];
                const auto& caOrigin = pRequest->Headers["origin"];

            } catch (Delphi::Exception::Exception &E) {
                ExceptionToJson(CHTTPReply::internal_server_error, E, pReply->Content);

                AConnection->SendReply(CHTTPReply::ok);
                Log()->Error(APP_LOG_EMERG, 0, E.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPModule::VerifyToken(const CString &Token) {

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

        bool CCPModule::CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization) {

            const auto &caHeaders = ARequest->Headers;
            const auto &caCookies = ARequest->Cookies;

            const auto &caAuthorization = caHeaders["Authorization"];

            if (caAuthorization.IsEmpty()) {

                Authorization.Username = caHeaders["Session"];
                Authorization.Password = caHeaders["Secret"];

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

        bool CCPModule::CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization) {

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

        bool CCPModule::Enabled() {
            if (m_ModuleStatus == msUnknown)
                m_ModuleStatus = Config()->IniFile().ReadBool(SectionName().c_str(), "enable", true) ? msEnabled : msDisabled;
            return m_ModuleStatus == msEnabled;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCPModule::CheckLocation(const CLocation &Location) {
            return CApostolModule::CheckLocation(Location);
        }
        //--------------------------------------------------------------------------------------------------------------

    }
}
}