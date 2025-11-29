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

            COCPPClientManager m_ClientManager;

            void BeforeRun() override;
            void AfterRun() override;

            void InitServer();

            COCPPClient *GetOCPPClient(const CLocation &URI);

            void CreateOCPPClient(const CString &Name);

            void InitializeActions(COCPPClient *AClient);
            void InitializeEvents(COCPPClient *AClient);

            static void LoadChargePointRequest(const CString &Prefix, const CString &Action, CJSON &Json);

            void Heartbeat(CDateTime Now);

            void OnChargePointMessageSOAP(CObject *Sender, const CSOAPMessage &Message) const;
            void OnChargePointMessageJSON(CObject *Sender, const CJSONMessage &Message) const;

            void OnCancelReservation(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnChangeAvailability(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnChangeConfiguration(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnClearCache(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnClearChargingProfile(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnDataTransfer(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnGetCompositeSchedule(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnGetConfiguration(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnGetDiagnostics(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnGetLocalListVersion(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnRemoteStartTransaction(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnRemoteStopTransaction(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnReserveNow(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnReset(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response);
            void OnSendLocalList(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnSetChargingProfile(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnTriggerMessage(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnUnlockConnector(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;
            void OnUpdateFirmware(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) const;

        protected:

            void DoTimer(CPollEventHandler *AHandler) override;

            void DoConnected(CObject *Sender) const;
            void DoDisconnected(CObject *Sender) const;

            void DoError(const Delphi::Exception::Exception &E);

            void DoException(CTCPConnection *AConnection, const Delphi::Exception::Exception &E);
            bool DoExecute(CTCPConnection *AConnection) override;

        public:

            explicit CCPEmulator(CCustomProcess* AParent, CApplication *AApplication);

            ~CCPEmulator() override = default;

            static CCPEmulator *CreateProcess(CCustomProcess *AParent, CApplication *AApplication) {
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
