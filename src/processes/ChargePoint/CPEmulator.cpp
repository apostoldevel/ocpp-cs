/*++

Program name:

  OCPP Charge Point Service

Module Name:

  CPEmulator.cpp

Notices:

  Process: Charging point emulator

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Core.hpp"
#include "CPEmulator.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <dirent.h>
//----------------------------------------------------------------------------------------------------------------------

#define CONFIG_SECTION_NAME "process/ChargePoint"
#define CHARGE_POINT_API_PORT 9221

extern "C++" {

namespace Apostol {

    namespace ChargePoint {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCPEmulator -----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCPEmulator::CCPEmulator(CCustomProcess *AParent, CApplication *AApplication):
                inherited(AParent, AApplication, ptCustom, "charging point emulator") {

            m_InitDate = 0;
            m_CheckDate = 0;
            
            m_Status = psStopped;

            InitializeServer(Application()->Title(), Config()->Listen(), CHARGE_POINT_API_PORT);
#ifdef WITH_POSTGRESQL
            InitializePQClient(Application()->Title(), 5, 10);
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::BeforeRun() {
            sigset_t set;

            Application()->Header(Application()->Name() + ": charging point emulator process");

            Log()->Debug(APP_LOG_DEBUG_CORE, MSG_PROCESS_START, GetProcessName(), Application()->Header().c_str());

            InitSignals();

            Reload();

            SetUser(Config()->User(), Config()->Group());

            ServerStart();
#ifdef WITH_POSTGRESQL
            PQClientStart("helper");
#endif
            SigProcMask(SIG_UNBLOCK, SigAddSet(&set));

            SetTimerInterval(1000);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::AfterRun() {
            CApplicationProcess::AfterRun();
#ifdef WITH_POSTGRESQL
            PQClientStop();
#endif
            ServerStop();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::Run() {

            while (!sig_exiting) {

                Log()->Debug(APP_LOG_DEBUG_EVENT, _T("charging point emulator process cycle"));

                try {
                    Server().Wait();
                } catch (std::exception &e) {
                    Log()->Error(APP_LOG_ERR, 0, _T("%s"), e.what());
                }

                if (sig_terminate || sig_quit) {
                    if (sig_quit) {
                        sig_quit = 0;
                        Log()->Debug(APP_LOG_DEBUG_EVENT, _T("gracefully shutting down"));
                        Application()->Header(_T("charging point emulator process is shutting down"));
                    }

                    if (!sig_exiting) {
                        sig_exiting = 1;
                        Log()->Debug(APP_LOG_DEBUG_EVENT, _T("exiting charging point emulator process"));
                    }
                }

                if (sig_reopen) {
                    sig_reopen = 0;

                    Log()->Debug(APP_LOG_DEBUG_EVENT, _T("charging point emulator process reconnect"));
                }
            }

            Log()->Debug(APP_LOG_DEBUG_EVENT, _T("stop charging point emulator process"));
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CCPEmulator::DoExecute(CTCPConnection *AConnection) {
            return CModuleProcess::DoExecute(AConnection);;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::Reload() {
            CServerProcess::Reload();

            m_ClientManager.Clear();

            Config()->IniFile().ReadSectionValues(CONFIG_SECTION_NAME, &m_Config);

            m_InitDate = 0;
            m_CheckDate = 0;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::InitServer() {

            CString prefix(Config()->Prefix());
            prefix << "cp/";

            DIR *dir;
            struct dirent *ent;
            if ((dir = opendir(prefix.c_str())) != nullptr) {
                while ((ent = readdir(dir)) != nullptr) {
                    if ((CompareString(ent->d_name, ".") != 0) && (CompareString(ent->d_name, "..") != 0)) {
                        CreateOCPPClient(ent->d_name);
                        m_Status = psRunning;
                    }
                }
                closedir (dir);
            } else {
                Log()->Error(APP_LOG_ALERT, 0, "Could not open directory \'%s\" service not running.", prefix.c_str());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPClient *CCPEmulator::GetOCPPClient(const CLocation &URI) {

            auto pClient = m_ClientManager.Add(URI);

            pClient->ClientName() = GApplication->Title();
            pClient->AutoConnect(false);
            pClient->PollStack(Server().PollStack());

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            pClient->OnVerbose([this](auto && Sender, auto && AConnection, auto && AFormat, auto && args) { DoVerbose(Sender, AConnection, AFormat, args); });
            pClient->OnAccessLog([this](auto && AConnection) { DoAccessLog(AConnection); });
            pClient->OnException([this](auto && AConnection, auto && AException) { DoException(AConnection, AException); });
            pClient->OnEventHandlerException([this](auto && AHandler, auto && AException) { DoServerEventHandlerException(AHandler, AException); });
            pClient->OnConnected([this](auto && Sender) { DoConnected(Sender); });
            pClient->OnDisconnected([this](auto && Sender) { DoDisconnected(Sender); });
            pClient->OnNoCommandHandler([this](auto && Sender, auto && AData, auto && AConnection) { DoNoCommandHandler(Sender, AData, AConnection); });
#else
            pClient->OnVerbose(std::bind(&CCPEmulator::DoVerbose, this, _1, _2, _3, _4));
            pClient->OnAccessLog(std::bind(&CCPEmulator::DoAccessLog, this, _1));
            pClient->OnException(std::bind(&CCPEmulator::DoException, this, _1, _2));
            pClient->OnEventHandlerException(std::bind(&CCPEmulator::DoServerEventHandlerException, this, _1, _2));
            pClient->OnConnected(std::bind(&CCPEmulator::DoConnected, this, _1));
            pClient->OnDisconnected(std::bind(&CCPEmulator::DoDisconnected, this, _1));
            pClient->OnNoCommandHandler(std::bind(&CCPEmulator::DoNoCommandHandler, this, _1, _2, _3));
#endif
            return pClient;
        }
        //--------------------------------------------------------------------------------------------------------------

        CString CCPEmulator::CheckOfNull(const CString &String) {
            if (!String.IsEmpty())
                return String;
            return {"<null>"};
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::LoadChargePointRequest(const CString &Prefix, const CString &Action, CJSON &Json) {

            CString sFileName(Prefix);

            sFileName << Action;
            sFileName << ".json";

            if (!FileExists(sFileName.c_str())) {
                throw Delphi::Exception::ExceptionFrm("Not found file: %s", sFileName.c_str());
            }

            Json.LoadFromFile(sFileName.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::CreateOCPPClient(const CString &Name) {

            CString prefix(Config()->Prefix());
            prefix << "cp/";
            prefix << Name;
            prefix << "/";

            CString fileName(prefix);
            fileName << "configuration.json";

            if (!FileExists(fileName.c_str())) {
                throw Delphi::Exception::ExceptionFrm("Not found file: %s", fileName.c_str());
            }

            CJSON json;
            json.LoadFromFile(fileName.c_str());

            const auto &connectors = json["connectorId"].AsArray();
            const auto &keys = json["configurationKey"].AsArray();

            CStringList params;
            for (int i = 0; i < keys.Count(); i++) {
                const auto &key = keys[i];
                params.AddPair(key["key"].AsString(), key["value"].AsString());
            }

            const auto &url = params["CentralSystemURL"];
            if (url.empty()) {
                throw Delphi::Exception::ExceptionFrm("[%s] Not found key \"%s\" in configuration file", Name.c_str(), "CentralSystemURL");
            }

            const auto &stationIdentity = params["StationIdentity"];

            Log()->Notice("[%s] Configuration file: %s", stationIdentity.c_str(), fileName.c_str());

            auto pClient = GetOCPPClient(CLocation(url + "/" + CHTTPServer::URLEncode(stationIdentity)));

            try {
                pClient->Prefix() = prefix;
                pClient->Identity() = stationIdentity;

                pClient->SetConnectors(connectors);
                pClient->SetConfigurationKeys(keys);

                pClient->ConnectorZero().Status(cpsAvailable);

                InitializeActions(pClient);
                InitializeEvents(pClient);

                pClient->Active(true);
            } catch (std::exception &e) {
                Log()->Error(APP_LOG_ERR, 0, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::InitializeActions(COCPPClient *AClient) {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            AClient->Actions().AddObject(_T("CancelReservation")     , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnCancelReservation(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("ChangeAvailability")    , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnChangeAvailability(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("ChangeConfiguration")   , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnChangeConfiguration(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("ClearCache")            , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnClearCache(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("ClearChargingProfile")  , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnClearChargingProfile(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("DataTransfer")          , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnDataTransfer(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("GetCompositeSchedule")  , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnGetCompositeSchedule(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("GetConfiguration")      , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnGetConfiguration(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("GetDiagnostics")        , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnGetDiagnostics(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("GetLocalListVersion")   , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnGetLocalListVersion(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("RemoteStartTransaction"), (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnRemoteStartTransaction(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("RemoteStopTransaction") , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnRemoteStopTransaction(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("ReserveNow")            , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnReserveNow(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("Reset")                 , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnReset(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("SendLocalList")         , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnSendLocalList(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("SetChargingProfile")    , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnSetChargingProfile(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("TriggerMessage")        , (CObject *) new CJSONActionHandler(true , [this](auto && Sender, auto && Request, auto && Response) { OnTriggerMessage(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("UnlockConnector")       , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnUnlockConnector(Sender, Request, Response); }));
            AClient->Actions().AddObject(_T("UpdateFirmware")        , (CObject *) new CJSONActionHandler(false, [this](auto && Sender, auto && Request, auto && Response) { OnUpdateFirmware(Sender, Request, Response); }));
#else
            AClient->Actions().AddObject(_T("CancelReservation")     , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnCancelReservation, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("ChangeAvailability")    , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnChangeAvailability, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("ChangeConfiguration")   , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnChangeConfiguration, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("ClearCache")            , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnClearCache, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("ClearChargingProfile")  , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnClearChargingProfile, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("DataTransfer")          , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnDataTransfer, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("GetCompositeSchedule")  , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnGetCompositeSchedule, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("GetConfiguration")      , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnGetConfiguration, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("GetDiagnostics")        , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnGetDiagnostics, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("GetLocalListVersion")   , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnGetLocalListVersion, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("RemoteStartTransaction"), (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnRemoteStartTransaction, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("RemoteStopTransaction") , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnRemoteStopTransaction, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("ReserveNow")            , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnReserveNow, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("Reset")                 , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnReset, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("SendLocalList")         , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnSendLocalList, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("SetChargingProfile")    , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnSetChargingProfile, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("TriggerMessage")        , (CObject *) new CJSONActionHandler(true , std::bind(&CCPEmulator::OnTriggerMessage, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("UnlockConnector")       , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnUnlockConnector, this, _1, _2, _3)));
            AClient->Actions().AddObject(_T("UpdateFirmware")        , (CObject *) new CJSONActionHandler(false, std::bind(&CCPEmulator::OnUpdateFirmware, this, _1, _2, _3)));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::InitializeEvents(COCPPClient *AClient) {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            AClient->OnMessageJSON([this](auto && Sender, auto && Message) { OnChargePointMessageJSON(Sender, Message); });
#else
            AClient->OnMessageJSON(std::bind(&CCPEmulator::OnChargePointMessageJSON, this, _1, _2));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::DoTimer(CPollEventHandler *AHandler) {
            uint64_t exp;

            auto pTimer = dynamic_cast<CEPollTimer *> (AHandler->Binding());
            pTimer->Read(&exp, sizeof(uint64_t));

            try {
                DoHeartbeat();
                CModuleProcess::HeartbeatModules();
            } catch (Delphi::Exception::Exception &E) {
                DoServerEventHandlerException(AHandler, E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::DoConnected(CObject *Sender) {
            auto pConnection = dynamic_cast<COCPPConnection *>(Sender);
            if (pConnection != nullptr) {
                auto pBinding = pConnection->Socket()->Binding();
                if (pBinding != nullptr) {
                    Log()->Notice(_T("[%s] [%s:%d] OCPP client connected."),
                                   pConnection->Identity().c_str(),
                                   pConnection->Socket()->Binding()->IP(),
                                   pConnection->Socket()->Binding()->Port());
                } else {
                    Log()->Notice(_T("[%s] OCPP client connected."),
                                   pConnection->Identity().c_str());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::DoDisconnected(CObject *Sender) {
            auto pConnection = dynamic_cast<COCPPConnection *>(Sender);
            if (pConnection != nullptr) {
                auto pBinding = pConnection->Socket()->Binding();
                if (pBinding != nullptr) {
                    Log()->Notice(_T("[%s] [%s:%d] OCPP client disconnected."),
                                   pConnection->Identity().c_str(),
                                   pConnection->Socket()->Binding()->IP(),
                                   pConnection->Socket()->Binding()->Port());
                } else {
                    Log()->Notice(_T("[%s] OCPP client disconnected."),
                                   pConnection->Identity().c_str());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::DoError(const Delphi::Exception::Exception &E) {

            m_InitDate = 0;
            m_CheckDate = 0;
            
            m_Status = psStopped;

            Log()->Error(APP_LOG_ERR, 0, "%s", E.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::DoHeartbeat() {
            const auto now = Now();

            if ((now >= m_InitDate)) {
                m_InitDate = now + (CDateTime) 30 / SecsPerDay; // 30 sec
                if (m_ClientManager.Count() == 0)
                    InitServer();
            }

            if (m_Status == psRunning) {
                if ((now >= m_CheckDate)) {
                    m_CheckDate = now + (CDateTime) 30 / SecsPerDay; // 30 sec
                    for (int i = 0; i < m_ClientManager.Count(); ++i) {
                        auto pClient = m_ClientManager.Items(i);
                        if (!pClient->Connected()) {
                            Log()->Notice(_T("[%s] Trying connect to %s."), pClient->Identity().c_str(), pClient->URI().href().c_str());
                            pClient->ConnectStart();
                        }
                    }
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::DoException(CTCPConnection *AConnection, const Delphi::Exception::Exception &E) {
            Log()->Error(APP_LOG_ERR, 0, "%s", E.what());
            sig_reopen = 1;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnChargePointMessageSOAP(CObject *Sender, const CSOAPMessage &Message) {
            auto pPoint = dynamic_cast<CChargingPoint *> (Sender);
            chASSERT(pPoint);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnChargePointMessageJSON(CObject *Sender, const CJSONMessage &Message) {
            auto pPoint = dynamic_cast<CChargingPoint *> (Sender);
            chASSERT(pPoint);
            Log()->Message("[%s] [%s] [%s] [%s] %s", pPoint->Identity().c_str(),
                           Message.UniqueId.c_str(),
                           Message.Action.IsEmpty() ? "Unknown" : Message.Action.c_str(),
                           COCPPMessage::MessageTypeIdToString(Message.MessageTypeId).c_str(),
                           Message.Payload.IsNull() ? "{}" : Message.Payload.ToString().c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnCancelReservation(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            if (!Request.Payload.HasOwnProperty("reservationId")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto reservationId = Request.Payload["reservationId"].AsInteger();

            CCancelReservationStatus status;

            try {
                status = pClient->CancelReservation(reservationId);
            } catch (Delphi::Exception::Exception &E) {
                pClient->SendProtocolError(Request.UniqueId, E.what());
                return;
            }

            Response.Payload.Object().AddPair("status", COCPPMessage::CancelReservationStatusToString(status));
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnChangeAvailability(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnChangeConfiguration(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            if (!Request.Payload.HasOwnProperty("key")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            if (!Request.Payload.HasOwnProperty("value")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto &key = Request.Payload["key"].AsString();
            const auto &value = Request.Payload["value"].AsString();

            const auto status = pClient->ChangeConfiguration(key, value);

            Response.Payload.Object().AddPair("status", COCPPMessage::ConfigurationStatusToString(status));
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnClearCache(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            const auto status = pClient->ClearCache();
            Response.Payload.Object().AddPair("status", COCPPMessage::ClearCacheStatusToString(status));
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnClearChargingProfile(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnDataTransfer(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);

            Response.Payload.Object().AddPair("status", "Accepted");

            if (Request.Payload.HasOwnProperty("data")) {
                Response.Payload.Object().AddPair("data", Request.Payload["data"]);
            }

            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnGetCompositeSchedule(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnGetConfiguration(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            CString key;

            if (Request.Payload.HasOwnProperty("key")) {
                key = Request.Payload["key"].AsString();
            }

            Response.Payload = pClient->ConfigurationToJson(key);

            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnGetDiagnostics(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnGetLocalListVersion(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnRemoteStartTransaction(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            if (!Request.Payload.HasOwnProperty("idTag")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto &idTag = Request.Payload["idTag"].AsString();

            int connectorId = 0;
            if (Request.Payload.HasOwnProperty("connectorId")) {
                connectorId = Request.Payload["connectorId"].AsInteger();
            }

            CJSON chargingProfile;
            if (Request.Payload.HasOwnProperty("chargingProfile")) {
                chargingProfile << Request.Payload["chargingProfile"];
            }

            CRemoteStartStopStatus status;

            try {
                status = pClient->RemoteStartTransaction(idTag, connectorId, chargingProfile);
            } catch (Delphi::Exception::Exception &E) {
                pClient->SendProtocolError(Request.UniqueId, E.what());
                return;
            }

            Response.Payload.Object().AddPair("status", COCPPMessage::RemoteStartStopStatusToString(status));
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnRemoteStopTransaction(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            if (!Request.Payload.HasOwnProperty("transactionId")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto transactionId = Request.Payload["transactionId"].AsInteger();

            CRemoteStartStopStatus status;

            try {
                status = pClient->RemoteStopTransaction(transactionId);
            } catch (Delphi::Exception::Exception &E) {
                pClient->SendProtocolError(Request.UniqueId, E.what());
                return;
            }

            Response.Payload.Object().AddPair("status", COCPPMessage::RemoteStartStopStatusToString(status));
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnReserveNow(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            if (!Request.Payload.HasOwnProperty("connectorId")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto connectorId = Request.Payload["connectorId"].AsInteger();

            if (!Request.Payload.HasOwnProperty("expiryDate")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto expiryDate = StrToDateTimeDef(Request.Payload["expiryDate"].AsString().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

            if (!Request.Payload.HasOwnProperty("idTag")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto &idTag = Request.Payload["idTag"].AsString();

            CString parentIdTag;
            if (Request.Payload.HasOwnProperty("parentIdTag")) {
                parentIdTag = Request.Payload["parentIdTag"].AsString();
            }

            if (!Request.Payload.HasOwnProperty("reservationId")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto reservationId = Request.Payload["reservationId"].AsInteger();

            CReservationStatus status;

            try {
                status = pClient->ReserveNow(connectorId, expiryDate, idTag, parentIdTag, reservationId);
            } catch (Delphi::Exception::Exception &E) {
                pClient->SendProtocolError(Request.UniqueId, E.what());
                return;
            }

            Response.Payload.Object().AddPair("status", COCPPMessage::ReservationStatusToString(status));
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnReset(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            if (!Request.Payload.HasOwnProperty("type")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            pClient->Reset();

            const auto type = COCPPMessage::StringToResetType(Request.Payload["type"].AsString());

            if (type == rtHard) {
                sync();
                if (reboot(RB_AUTOBOOT) == -1) {
                    throw EOSError(errno, "reboot(RB_AUTOBOOT) failed");
                }
            } else {
                SignalRestart();
            }

            Response.Payload.Object().AddPair("status", "Accepted");

            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnSendLocalList(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnSetChargingProfile(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnTriggerMessage(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);

            chASSERT(pClient);

            if (!Request.Payload.HasOwnProperty("requestedMessage")) {
                pClient->SendProtocolError(Request.UniqueId, OCPP_PROTOCOL_ERROR_MESSAGE);
                return;
            }

            const auto requestedMessage = COCPPMessage::StringToMessageTrigger(Request.Payload["requestedMessage"].AsString());

            int connectorId = 0;
            if (Request.Payload.HasOwnProperty("connectorId")) {
                connectorId = Request.Payload["connectorId"].AsInteger();
            }

            CTriggerMessageStatus status;

            try {
                status = pClient->TriggerMessage(requestedMessage, connectorId);
            } catch (Delphi::Exception::Exception &E) {
                pClient->SendProtocolError(Request.UniqueId, E.what());
                return;
            }

            Response.Payload.Object().AddPair("status", COCPPMessage::TriggerMessageStatusToString(status));
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnUnlockConnector(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCPEmulator::OnUpdateFirmware(CObject *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
            auto pClient = dynamic_cast<COCPPClient *> (Sender);
            chASSERT(pClient);
            LoadChargePointRequest(pClient->Prefix(), Request.Action, Response.Payload);
            pClient->SendMessage(Response);
        }
        //--------------------------------------------------------------------------------------------------------------

    }
}

}
