/*++

Program name:

  OCPP Central System Service

Module Name:

  WebService.hpp

Notices:

  Module WebService 

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_WEBSERVICE_HPP
#define APOSTOL_WEBSERVICE_HPP
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace CSService {

        enum CAuthorizationSchemes { asUnknown, asBasic };

        typedef struct CAuthorization {

            CAuthorizationSchemes Schema;

            CString Username;
            CString Password;

            CAuthorization(): Schema(asUnknown) {

            }

            explicit CAuthorization(const CString& String): CAuthorization() {
                Parse(String);
            }

            void Parse(const CString& String) {
                if (String.SubString(0, 5).Lower() == "basic") {
                    const CString LPassphrase(base64_decode(String.SubString(6)));

                    const size_t LPos = LPassphrase.Find(':');
                    if (LPos == CString::npos)
                        throw Delphi::Exception::Exception("Authorization error: Incorrect passphrase.");

                    Schema = asBasic;
                    Username = LPassphrase.SubString(0, LPos);
                    Password = LPassphrase.SubString(LPos + 1);

                    if (Username.IsEmpty() || Password.IsEmpty())
                        throw Delphi::Exception::Exception("Authorization error: Username and password has not be empty.");
                } else {
                    throw Delphi::Exception::Exception("Authorization error: Unknown schema.");
                }
            }

            CAuthorization &operator << (const CString& String) {
                Parse(String);
                return *this;
            }

        } CAuthorization;

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCSService: public CApostolModule {
        private:

            CChargingPointManager *m_CPManager;

            static void DebugRequest(CRequest *ARequest);
            static void DebugReply(CReply *AReply);
            static void DebugConnection(CHTTPServerConnection *AConnection);

            static void ExceptionToJson(int ErrorCode, const std::exception &AException, CString& Json);

            bool QueryStart(CHTTPServerConnection *AConnection, const CStringList& SQL);
            bool DBParse(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Action, const CJSON &Payload);

        protected:

            static void DoWWW(CHTTPServerConnection *AConnection);

            void DoGet(CHTTPServerConnection *AConnection);
            void DoPost(CHTTPServerConnection *AConnection);

            void DoHTTP(CHTTPServerConnection *AConnection);
            void DoWebSocket(CHTTPServerConnection *AConnection);

            void DoPointDisconnected(CObject *Sender);

            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) override;

        public:

            explicit CCSService(CModuleManager *AManager);

            ~CCSService() override;

            static class CCSService *CreateModule(CModuleManager *AManager) {
                return new CCSService(AManager);
            }

            void InitMethods() override;

            void BeforeExecute(Pointer Data) override;
            void AfterExecute(Pointer Data) override;

            void Execute(CHTTPServerConnection *AConnection) override;

            bool CheckUserAgent(const CString& Value) override;

        };
    }
}

using namespace Apostol::CSService;
}
#endif //APOSTOL_WEBSERVICE_HPP
