/*++

Program name:

  OCPP Charge Point Service

Module Name:

  CPEmulator.hpp

Notices:

  Process: Charging point emulator

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef OCPP_CHARGING_POINT_EMULATOR_HPP
#define OCPP_CHARGING_POINT_EMULATOR_HPP
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace ChargePoint {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCPEmulator -----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCPEmulator: public CApplicationProcess, public CModuleProcess {
            typedef CApplicationProcess inherited;

        private:

            CProcessStatus m_Status;

            CStringList m_Config;

            CDateTime m_InitDate;
            CDateTime m_CheckDate;

            CCPModule* m_pCPModule;

            COCPPClientManager m_ClientManager;

            void BeforeRun() override;
            void AfterRun() override;

            void InitServer();

            COCPPClient *GetOCPPClient(const CLocation &URI);

            void CreateOCPPClient(const CString &Name);

            void InitializeActions(COCPPClient *AClient);
            void InitializeEvents(COCPPClient *AClient);

            static CString CheckOfNull(const CString &String);

            static void LoadChargePointRequest(const CString &Prefix, const CString &Action, CJSON &Json);

            void OnChargePointMessageSOAP(CObject *Sender, const CSOAPMessage &Message);
            void OnChargePointMessageJSON(CObject *Sender, const CJSONMessage &Message);

            void OnCancelReservation(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnChangeAvailability(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnChangeConfiguration(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnClearCache(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnClearChargingProfile(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnDataTransfer(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnGetCompositeSchedule(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnGetConfiguration(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnGetDiagnostics(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnGetLocalListVersion(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnRemoteStartTransaction(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnRemoteStopTransaction(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnReserveNow(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnReset(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnSendLocalList(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnSetChargingProfile(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnTriggerMessage(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnUnlockConnector(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnUpdateFirmware(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);

        protected:

            void DoTimer(CPollEventHandler *AHandler) override;

            void DoConnected(CObject *Sender);
            void DoDisconnected(CObject *Sender);

            void DoHeartbeat();

            void DoError(const Delphi::Exception::Exception &E);

            void DoException(CTCPConnection *AConnection, const Delphi::Exception::Exception &E);
            bool DoExecute(CTCPConnection *AConnection) override;

        public:

            explicit CCPEmulator(CCustomProcess* AParent, CApplication *AApplication);

            ~CCPEmulator() override;

            static class CCPEmulator *CreateProcess(CCustomProcess *AParent, CApplication *AApplication) {
                return new CCPEmulator(AParent, AApplication);
            }

            void Run() override;
            void Reload() override;

        };
        //--------------------------------------------------------------------------------------------------------------

    }
}

using namespace Apostol::ChargePoint;
}
#endif //OCPP_CHARGING_POINT_EMULATOR_HPP
