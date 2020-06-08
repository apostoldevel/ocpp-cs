/*++

Library name:

  apostol-core

Module Name:

  CertificateDownloader.cpp

Notices:

  Apostol application

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Core.hpp"
#include "CertificateDownloader.hpp"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Helpers {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCertificateDownloader ------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCertificateDownloader::CCertificateDownloader(CModuleProcess *AProcess) : CApostolModule(AProcess, "certificate downloader") {
            m_SyncPeriod = 30;
            CCertificateDownloader::InitMethods();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCertificateDownloader::InitMethods() {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoOptions(Connection); }));
            m_pMethods->AddObject(_T("HEAD")   , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoHead(Connection); }));
            m_pMethods->AddObject(_T("GET")    , (CObject *) new CMethodHandler(false, [this](auto && Connection) { DoGet(Connection); }));
            m_pMethods->AddObject(_T("POST")   , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("PUT")    , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("DELETE") , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("TRACE")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("PATCH")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
#else
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true, std::bind(&CCertificateDownloader::DoOptions, this, _1)));
            m_pMethods->AddObject(_T("HEAD"), (CObject *) new CMethodHandler(true, std::bind(&CCertificateDownloader::DoHead, this, _1)));
            m_pMethods->AddObject(_T("GET"), (CObject *) new CMethodHandler(false, std::bind(&CCertificateDownloader::DoGet, this, _1)));
            m_pMethods->AddObject(_T("POST"), (CObject *) new CMethodHandler(false, std::bind(&CCertificateDownloader::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PUT"), (CObject *) new CMethodHandler(false, std::bind(&CCertificateDownloader::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("DELETE"), (CObject *) new CMethodHandler(false, std::bind(&CCertificateDownloader::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("TRACE"), (CObject *) new CMethodHandler(false, std::bind(&CCertificateDownloader::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PATCH"), (CObject *) new CMethodHandler(false, std::bind(&CCertificateDownloader::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CCertificateDownloader::MethodNotAllowed, this, _1)));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_POSTGRESQL
        void CCertificateDownloader::DoPostgresQueryExecuted(CPQPollQuery *APollQuery) {
            clock_t start = clock();

            log_debug1(APP_LOG_DEBUG_CORE, Log(), 0, _T("Query executed runtime: %.2f ms."), (double) ((clock() - start) / (double) CLOCKS_PER_SEC * 1000));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCertificateDownloader::DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) {

        }
        //--------------------------------------------------------------------------------------------------------------
#endif
        void CCertificateDownloader::FetchCerts(CAuthParam &Key) {

            Key.Status = CAuthParam::ksError;
#ifdef WITH_CURL
            const auto& URI = Key.CertURI();

            if (URI.IsEmpty()) {
                Log()->Error(APP_LOG_WARN, 0, _T("Certificate URI is empty."));
                return;
            }

            Log()->Debug(0, _T("Trying to fetch public keys from: %s"), URI.c_str());

            CString jsonString;

            try {
                m_Curl.Reset();
                m_Curl.Send(URI, jsonString);

                Key.StatusTime = Now();

                /* Check for errors */
                if (m_Curl.Code() == CURLE_OK) {
                    if (!jsonString.IsEmpty()) {
                        Key.Keys << jsonString;
                        Key.Status = CAuthParam::ksSuccess;
                    }
                } else {
                    Log()->Error(APP_LOG_EMERG, 0, _T("[CURL] Failed: %s (%s)."), m_Curl.GetErrorMessage().c_str(), URI.c_str());
                }
            } catch (Delphi::Exception::Exception &e) {
                Log()->Error(APP_LOG_EMERG, 0, _T("[CURL] Error: %s"), e.what());
            }
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCertificateDownloader::FetchProviders() {
            auto& Params = Server().AuthParams();
            for (int i = 0; i < Params.Count(); i++) {
                auto& Key = Params[i].Value();
                if (Key.Status == CAuthParam::ksUnknown) {
                    Key.StatusTime = Now();
                    Key.Status = CAuthParam::ksFetching;
                    FetchCerts(Key);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCertificateDownloader::CheckProviders() {
            auto& Params = Server().AuthParams();
            for (int i = 0; i < Params.Count(); i++) {
                auto& Key = Params[i].Value();
                if (Key.Status == CAuthParam::ksSuccess) {
                    Key.StatusTime = Now();
                    Key.Status = CAuthParam::ksSaved;
                    SaveKeys(Key);
                }

                if (Key.Status != CAuthParam::ksUnknown && (Now() - Key.StatusTime >= (CDateTime) (m_SyncPeriod * 60 / 86400))) {
                    Key.StatusTime = Now();
                    Key.Status = CAuthParam::ksUnknown;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCertificateDownloader::SaveKeys(const CAuthParam &Key) {
            const CString pathCerts = Config()->Prefix() + _T("certs/");
            const CString lockFile = pathCerts + "lock";
            if (!FileExists(lockFile.c_str())) {
                CFile Lock(lockFile.c_str(), FILE_CREATE_OR_OPEN);
                Lock.Open();
                Key.Keys.SaveToFile(CString(pathCerts + Key.Provider).c_str());
                Lock.Close(true);
                if (unlink(lockFile.c_str()) == FILE_ERROR) {
                    Log()->Error(APP_LOG_ALERT, errno, _T("Could not delete file: \"%s\" error: "), lockFile.c_str());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCertificateDownloader::Heartbeat() {
            FetchProviders();
            CheckProviders();
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCertificateDownloader::IsEnabled() {
            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCertificateDownloader::CheckUserAgent(const CString& Value) {
            return IsEnabled();
        }
    }
}

}
