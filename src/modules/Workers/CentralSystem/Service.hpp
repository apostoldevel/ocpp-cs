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

        typedef TPairs<CStringList> COperations;

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCSService: public CApostolModule {
        private:

            COperations m_Operations;

            CChargingPointManager *m_pManager;

            bool m_bParseInDataBase;
            bool m_bSetConnected;

            void InitMethods() override;

            void InitOperations();

            void ParseJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Action, const CJSON &Payload);

            static bool CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization);
            void VerifyToken(const CString &Token);

            void SetPointConnected(CChargingPoint *APoint, bool Value);
            static void DoWebSocketError(CHTTPServerConnection *AConnection, const Delphi::Exception::Exception &E);

        protected:

            void DoWebSocket(CHTTPServerConnection *AConnection);
            void DoAPI(CHTTPServerConnection *AConnection);

            void DoGet(CHTTPServerConnection *AConnection) override;
            void DoPost(CHTTPServerConnection *AConnection);

            void DoOCPP(CHTTPServerConnection *AConnection);

            void DoPointConnected(CChargingPoint *APoint);
            void DoPointDisconnected(CObject *Sender);

            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) override;

        public:

            explicit CCSService(CModuleProcess *AProcess);

            ~CCSService() override;

            static class CCSService *CreateModule(CModuleProcess *AProcess) {
                return new CCSService(AProcess);
            }

            bool CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization);

            void Initialization(CModuleProcess *AProcess) override;

            void Heartbeat() override;
            bool Execute(CHTTPServerConnection *AConnection) override;

            bool Enabled() override;

        };
    }
}

using namespace Apostol::Workers;
}
#endif //APOSTOL_WEBSERVICE_HPP
