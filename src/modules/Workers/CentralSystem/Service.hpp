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

    namespace Workers {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCSService: public CApostolModule {
        private:

            CChargingPointManager *m_CPManager;

            void InitMethods() override;

            static void ReplyError(CHTTPServerConnection *AConnection, CReply::CStatusType ErrorCode, const CString &Message);

            static bool CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization);

            bool QueryStart(CHTTPServerConnection *AConnection, const CStringList& SQL);
            bool DBParse(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Action, const CJSON &Payload);

        protected:

            void DoOCPP(CHTTPServerConnection *AConnection);
            void DoAPI(CHTTPServerConnection *AConnection);

            void DoGet(CHTTPServerConnection *AConnection) override;
            void DoPost(CHTTPServerConnection *AConnection);

            void DoWebSocket(CHTTPServerConnection *AConnection);

            void DoPointDisconnected(CObject *Sender);

            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) override;

        public:

            explicit CCSService(CModuleProcess *AProcess);

            ~CCSService() override;

            static class CCSService *CreateModule(CModuleProcess *AProcess) {
                return new CCSService(AProcess);
            }

            void Heartbeat() override;
            void Execute(CHTTPServerConnection *AConnection) override;

            bool IsEnabled() override;
            bool CheckUserAgent(const CString& Value) override;

        };
    }
}

using namespace Apostol::Workers;
}
#endif //APOSTOL_WEBSERVICE_HPP
