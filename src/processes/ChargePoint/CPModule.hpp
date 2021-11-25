/*++

Program name:

  OCPP Charge Point Service

Module Name:

  CPEmulator.hpp

Notices:

  Module: Charge Point Web Server

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef OCPP_CHARGE_POINT_MODULE_HPP
#define OCPP_CHARGE_POINT_MODULE_HPP
//----------------------------------------------------------------------------------------------------------------------

#include "CPClient.hpp"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace CPModule {

        //--------------------------------------------------------------------------------------------------------------

        //-- CModuleConnection -----------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CModuleConnection: public CPollConnection {
        private:

            CString m_Action {};
            CString m_URL {};

            CHTTPServerConnection *m_pConnection;

        public:

            explicit CModuleConnection(const CString& Address, CHTTPServerConnection *AConnection, CPollManager *AManager = nullptr):
                    CPollConnection(AManager) {
                m_pConnection = AConnection;
            }

            CHTTPServerConnection *Connection() { return m_pConnection; }

            CString& Action() { return m_Action; };
            const CString& Action() const { return m_Action; }

            CString& URL() { return m_URL; };
            const CString& URL() const { return m_URL; }

            void HostToURL(const CString& Host);

            void Close() override {
                Connection()->Disconnect();
            }

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CCPModule -------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCPModule: public CApostolModule {
        private:

            void InitMethods() override;

            static bool CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization);
            void VerifyToken(const CString &Token);

        protected:

            void DoGet(CHTTPServerConnection *AConnection) override;
            void DoPost(CHTTPServerConnection *AConnection);

        public:

            explicit CCPModule(CModuleProcess *AProcess);

            ~CCPModule() override = default;

            static class CCPModule *CreateModule(CModuleProcess *AProcess) {
                return new CCPModule(AProcess);
            }

            bool Enabled() override;

            bool CheckLocation(const CLocation &Location) override;
            bool CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization);

        };
    }
}

using namespace Apostol::CPModule;
}
#endif //OCPP_CHARGE_POINT_MODULE_HPP
