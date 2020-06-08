/*++

Program name:

  Apostol Web Service

Module Name:

  CertificateDownloader.hpp

Notices:

  Module Certificate Downloader

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_CERTIFICATEDOWNLOADER_HPP
#define APOSTOL_CERTIFICATEDOWNLOADER_HPP
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Helpers {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCertificateDownloader ------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCertificateDownloader: public CApostolModule {
        private:
#ifdef WITH_CURL
            CCurlApi m_Curl;
#endif
            CDateTime m_SyncPeriod;

            void FetchCerts(CAuthParam &Key);

            void FetchProviders();
            void CheckProviders();

            static void SaveKeys(const CAuthParam &Key);

            void InitMethods() override;

        protected:
#ifdef WITH_POSTGRESQL
            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) override;
#endif
        public:

            explicit CCertificateDownloader(CModuleProcess *AProcess);

            ~CCertificateDownloader() override = default;

            static class CCertificateDownloader *CreateModule(CModuleProcess *AProcess) {
                return new CCertificateDownloader(AProcess);
            }

            void Heartbeat() override;

            bool IsEnabled() override;
            bool CheckUserAgent(const CString& Value) override;

        };

    }
}

using namespace Apostol::Helpers;
}
#endif //APOSTOL_CERTIFICATEDOWNLOADER_HPP
