/*++

Program name:

  OCPP Charge Point Service

Module Name:

  CPClient.cpp

Notices:

  Charge Point Client

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Core.hpp"
#include "CPClient.hpp"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace ChargePoint {

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPClient -----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        COCPPClient::COCPPClient(): CGlobalComponent(), CHTTPClient(), CChargingPoint() {
            m_pTimer = nullptr;
            m_ErrorCount = -1;
            m_TimerInterval = 0;
            m_PingDateTime = 0;
            m_HeartbeatDateTime = 0;
            m_MeterValueDateTime = 0;
            m_BootNotificationDateTime = 0;
            m_HeartbeatInterval = 600;
            m_Key = GetUID(16);
        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPClient::COCPPClient(const CLocation &URI): CGlobalComponent(), CHTTPClient(URI.hostname, URI.port),
                CChargingPoint(), m_URI(URI) {

            m_pTimer = nullptr;
            m_ErrorCount = -1;
            m_TimerInterval = 0;
            m_PingDateTime = 0;
            m_HeartbeatDateTime = 0;
            m_MeterValueDateTime = 0;
            m_BootNotificationDateTime = 0;
            m_HeartbeatInterval = 600;
            m_Key = GetUID(16);
        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPClient::~COCPPClient() {
            SwitchConnection(nullptr);
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::IncErrorCount() {
            if (m_ErrorCount == UINT32_MAX)
                m_ErrorCount = 0;
            m_ErrorCount++;
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::SetTimerInterval(int Value) {
            if (m_TimerInterval != Value) {
                m_TimerInterval = Value;
                UpdateTimer();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::UpdateTimer() {
            if (m_pTimer == nullptr) {
                m_pTimer = CEPollTimer::CreateTimer(CLOCK_MONOTONIC, TFD_NONBLOCK);
                m_pTimer->AllocateTimer(m_pEventHandlers, m_TimerInterval, m_TimerInterval);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                m_pTimer->OnTimer([this](auto && AHandler) { DoTimer(AHandler); });
#else
                m_pTimer->OnTimer(std::bind(&COCPPClient::DoTimer, this, _1));
#endif
            } else {
                m_pTimer->SetTimer(m_TimerInterval, m_TimerInterval);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoDebugWait(CObject *Sender) {
            auto pConnection = dynamic_cast<COCPPConnection *> (Sender);
            if (Assigned(pConnection))
                DebugRequest(pConnection->Request());
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoDebugRequest(CObject *Sender) {
            auto pConnection = dynamic_cast<COCPPConnection *> (Sender);
            if (Assigned(pConnection)) {
                if (pConnection->Protocol() == pHTTP) {
                    DebugRequest(pConnection->Request());
                } else {
                    WSDebug(pConnection->WSRequest());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoDebugReply(CObject *Sender) {
            auto pConnection = dynamic_cast<COCPPConnection *> (Sender);
            if (Assigned(pConnection)) {
                if (pConnection->Protocol() == pHTTP) {
                    DebugReply(pConnection->Reply());
                } else {
                    WSDebug(pConnection->WSReply());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoConnectStart(CIOHandlerSocket *AIOHandler, CPollEventHandler *AHandler) {
            auto pConnection = new COCPPConnection(this);
            pConnection->IOHandler(AIOHandler);
            pConnection->AutoFree(false);
            AHandler->Binding(pConnection);
            SwitchConnection(pConnection);
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoConnect(CPollEventHandler *AHandler) {
            auto pConnection = dynamic_cast<COCPPConnection *> (AHandler->Binding());

            if (pConnection == nullptr) {
                AHandler->Stop();
                return;
            }

            try {
                auto pIOHandler = (CIOHandlerSocket *) pConnection->IOHandler();

                if (pIOHandler->Binding()->CheckConnection()) {
                    ClearErrorCount();
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                    pConnection->OnDisconnected([this](auto &&Sender) { DoDisconnected(Sender); });
                    pConnection->OnWaitRequest([this](auto &&Sender) { DoDebugWait(Sender); });
                    pConnection->OnRequest([this](auto &&Sender) { DoDebugRequest(Sender); });
                    pConnection->OnReply([this](auto &&Sender) { DoDebugReply(Sender); });
#else
                    pConnection->OnDisconnected(std::bind(&COCPPClient::DoDisconnected, this, _1));
                    pConnection->OnWaitRequest(std::bind(&COCPPClient::DoDebugWait, this, _1));
                    pConnection->OnRequest(std::bind(&COCPPClient::DoDebugRequest, this, _1));
                    pConnection->OnReply(std::bind(&COCPPClient::DoDebugReply, this, _1));
#endif
                    AHandler->Start(etIO);

                    pConnection->Identity() = Identity();

                    DoConnected(pConnection);
                    DoHandshake(pConnection);
                }
            } catch (Delphi::Exception::Exception &E) {
                IncErrorCount();
                AHandler->Stop();
                SwitchConnection(nullptr);
                throw ESocketError(E.ErrorCode(), "Connection failed ");
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoWebSocket(CHTTPClientConnection *AConnection) {

            auto pWSRequest = AConnection->WSRequest();
            auto pWSReply = AConnection->WSReply();

            pWSReply->Clear();

            CJSONMessage jmRequest;
            CJSONMessage jmResponse;

            const CString csRequest(pWSRequest->Payload());

            CJSONProtocol::Request(csRequest, jmRequest);

            try {
                if (jmRequest.MessageTypeId == ChargePoint::mtCall) {
                    int i;
                    CJSONActionHandler *pHandler;
                    for (i = 0; i < m_Actions.Count(); ++i) {
                        pHandler = (CJSONActionHandler *) m_Actions.Objects(i);
                        if (pHandler->Allow()) {
                            const auto &action = m_Actions.Strings(i);
                            if (action == jmRequest.Action) {
                                CJSONProtocol::PrepareResponse(jmRequest, jmResponse);
                                DoMessageJSON(jmRequest);
                                pHandler->Handler(this, jmRequest, jmResponse);
                                break;
                            }
                        }
                    }

                    if (i == m_Actions.Count()) {
                        SendNotSupported(jmRequest.UniqueId, CString().Format("Action %s not supported.", jmRequest.Action.c_str()));
                    }
                } else {
                    auto pHandler = m_Messages.FindMessageById(jmRequest.UniqueId);
                    if (Assigned(pHandler)) {
                        jmRequest.Action = pHandler->Message().Action;
                        DoMessageJSON(jmRequest);
                        pHandler->Handler(AConnection);
                        delete pHandler;
                    }
                }
            } catch (std::exception &e) {
                Log()->Error(APP_LOG_ERR, 0, e.what());
                SendInternalError(jmRequest.UniqueId, e.what());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoHTTP(CHTTPClientConnection *AConnection) {
            auto pReply = AConnection->Reply();

            if (pReply->Status == CHTTPReply::switching_protocols) {
#ifdef _DEBUG
                WSDebugConnection(AConnection);
#endif
                AConnection->SwitchingProtocols(pWebSocket);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool COCPPClient::DoExecute(CTCPConnection *AConnection) {
            auto pConnection = dynamic_cast<CHTTPClientConnection *> (AConnection);
            if (pConnection->Protocol() == pWebSocket) {
                DoWebSocket(pConnection);
            } else {
                DoHTTP(pConnection);
            }
            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoHandshake(COCPPConnection *AConnection) {
            auto pRequest = AConnection->Request();

            pRequest->AddHeader("Sec-WebSocket-Version", "13");
            pRequest->AddHeader("Sec-WebSocket-Key", base64_encode(m_Key));
            pRequest->AddHeader("Sec-WebSocket-Protocol", "ocpp16");
            pRequest->AddHeader("Upgrade", "websocket");

            CHTTPRequest::Prepare(pRequest, _T("GET"), m_URI.href().c_str(), nullptr, "Upgrade");

            AConnection->SendRequest();
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::Initialize() {
            const auto &heartbeatInterval = ConfigurationKeys()["HeartbeatInterval"].value;
            if (!heartbeatInterval.IsEmpty()) {
                m_HeartbeatInterval = StrToIntDef(heartbeatInterval.c_str(), 600);
            }

            SetTimerInterval(1000);
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoTimer(CPollEventHandler *AHandler) {
            uint64_t exp;

            auto pTimer = dynamic_cast<CEPollTimer *> (AHandler->Binding());
            pTimer->Read(&exp, sizeof(uint64_t));

            DoHeartbeat();
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoHeartbeat() {
            const auto now = UTC();

            if (now >= m_PingDateTime) {
                m_PingDateTime = now + (CDateTime) 60 / SecsPerDay; // 60 sec
                Ping();
            } else if (BootNotificationStatus() != rsAccepted) {
                if (now >= m_BootNotificationDateTime) {
                    m_BootNotificationDateTime = now + (CDateTime) 30 / SecsPerDay; // 30 sec
                    SendBootNotification();
                }
            } else {
                for (int i = 0; i < Connectors().Count(); ++i) {
                    auto& connector = Connectors()[i];
                    if (connector.Status() == cpsCharging) {
                        connector.IncMeterValue(10);
                    } else if (connector.Status() == cpsFinishing) {
                        connector.Status(cpsAvailable);
                    } else if (connector.Status() == cpsReserved && connector.ExpiryDate() <= now) {
                        connector.CancelReservation(connector.ReservationId());
                    }
                }

                bool meterSend = false;

                if (now >= m_MeterValueDateTime) {
                    m_MeterValueDateTime = now + (CDateTime) 60 / SecsPerDay; // 60 sec
                    meterSend = MeterValues();
                }

                if (!meterSend && now >= m_HeartbeatDateTime) {
                    m_HeartbeatDateTime = now + (CDateTime) m_HeartbeatInterval / SecsPerDay;
                    SendHeartbeat();
                }
            }
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPClientItem -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        COCPPClientItem::COCPPClientItem(COCPPClientManager *AManager): CCollectionItem(AManager), COCPPClient() {

        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPClientItem::COCPPClientItem(COCPPClientManager *AManager, const CLocation &URI):
                CCollectionItem(AManager), COCPPClient(URI) {

        }

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPClientManager ----------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        COCPPClientItem *COCPPClientManager::GetItem(int Index) const {
            return (COCPPClientItem *) inherited::GetItem(Index);
        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPClientItem *COCPPClientManager::Add(const CLocation &URI) {
            return new COCPPClientItem(this, URI);
        }

    }
}

}
