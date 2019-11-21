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

        typedef struct CDataBase {
            CString Username;
            CString Password;
            CString Session;
        } CDataBase;

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCSService: public CApostolModule {
        private:

            CChargingPointManager *m_CPManager;

            static void PQResultToJson(CPQResult *Result, CString& Json);

            void QueryToJson(CPQPollQuery *Query, CString& Json);

            bool QueryStart(CHTTPServerConnection *AConnection, const CStringList& SQL);

            static void DebugRequest(CRequest *ARequest);

            static void DebugReply(CReply *AReply);

        protected:

            static void DoWWW(CHTTPServerConnection *AConnection);

            void DoGet(CHTTPServerConnection *AConnection);
            void DoPost(CHTTPServerConnection *AConnection);

            void DoHTTP(CHTTPServerConnection *AConnection);
            void DoWebSocket(CHTTPServerConnection *AConnection);

            void DoPointDisconnected(CObject *Sender);

            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) override;

            static void ExceptionToJson(int ErrorCode, Delphi::Exception::Exception *AException, CString& Json);

            bool APIRun(CHTTPServerConnection *AConnection, const CString &Route, const CString &jsonString, const CDataBase &DataBase);

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
