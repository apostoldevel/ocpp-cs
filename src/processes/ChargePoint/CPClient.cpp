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

        void COCPPClient::SetTimerInterval(const int Value) {
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
            const auto pConnection = dynamic_cast<COCPPConnection *> (Sender);
            if (Assigned(pConnection))
                DebugRequest(pConnection->Request());
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoDebugRequest(CObject *Sender) {
            const auto pConnection = dynamic_cast<COCPPConnection *> (Sender);
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
            const auto pConnection = dynamic_cast<COCPPConnection *> (Sender);
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
            const auto pConnection = new COCPPConnection(this);
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
                const auto pIOHandler = dynamic_cast<CIOHandlerSocket *>(pConnection->IOHandler());

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
                throw ESocketError(E.ErrorCode(), "Connection failed");
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoWebSocket(CHTTPClientConnection *AConnection) {

            const auto &caWSRequest = AConnection->WSRequest();
            auto &WSReply = AConnection->WSReply();

            WSReply.Clear();

            CJSONMessage jmRequest;

            const CString csRequest(caWSRequest.Payload());

            CJSONProtocol::Request(csRequest, jmRequest);

            try {
                if (jmRequest.MessageTypeId == ChargePoint::mtCall) {
                    CJSONMessage jmResponse;
                    int i;
                    for (i = 0; i < m_Actions.Count(); ++i) {
                        const auto pHandler = (CJSONActionHandler *) m_Actions.Objects(i);
                        if (pHandler != nullptr && pHandler->Allow()) {
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
                    const auto pHandler = m_Messages.FindMessageById(jmRequest.UniqueId);
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
            const auto &Reply = AConnection->Reply();

            if (Reply.Status == CHTTPReply::switching_protocols) {
#ifdef _DEBUG
                WSDebugConnection(AConnection);
#endif
                AConnection->SwitchingProtocols(pWebSocket);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool COCPPClient::DoExecute(CTCPConnection *AConnection) {
            const auto pConnection = dynamic_cast<CHTTPClientConnection *> (AConnection);
            if (pConnection->Protocol() == pWebSocket) {
                DoWebSocket(pConnection);
            } else {
                DoHTTP(pConnection);
            }
            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoHandshake(COCPPConnection *AConnection) {
            auto &Request = AConnection->Request();

            Request.AddHeader("Sec-WebSocket-Version", "13");
            Request.AddHeader("Sec-WebSocket-Key", base64_encode(m_Key));
            Request.AddHeader("Sec-WebSocket-Protocol", "ocpp16");
            Request.AddHeader("Upgrade", "websocket");

            CHTTPRequest::Prepare(Request, _T("GET"), m_URI.href().c_str(), nullptr, "Upgrade");

            AConnection->SendRequest(true);
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

            const auto pTimer = dynamic_cast<CEPollTimer *> (AHandler->Binding());
            pTimer->Read(&exp, sizeof(uint64_t));

            DoHeartbeat(UTC());
        }
        //--------------------------------------------------------------------------------------------------------------

        void COCPPClient::DoHeartbeat(const CDateTime Now) {
            if (Now >= m_PingDateTime) {
                m_PingDateTime = Now + static_cast<CDateTime>(60) / SecsPerDay; // 60 sec
                Ping();
            } else if (BootNotificationStatus() != rsAccepted) {
                if (Now >= m_BootNotificationDateTime) {
                    m_BootNotificationDateTime = Now + static_cast<CDateTime>(30) / SecsPerDay; // 30 sec
                    SendBootNotification();
                }
            } else {
                for (int i = 0; i < Connectors().Count(); ++i) {
                    auto& connector = Connectors()[i];
                    if (connector.Status() == cpsCharging) {
                        connector.IncMeterValue(10);
                    } else if (connector.Status() == cpsFinishing) {
                        const auto &caFinishingTimeout = ConfigurationKeys()["FinishingTimeout"].value;
                        const auto timeout = caFinishingTimeout.empty() ? 60 : StrToIntDef(caFinishingTimeout.c_str(), 60);
                        if ((timeout > 0) && ((connector.StatusUpdated() + static_cast<CDateTime>(timeout) / SecsPerDay) <= Now)) {
                            connector.Status(cpsAvailable);
                        }
                    } else if (connector.Status() == cpsReserved && connector.ExpiryDate() <= Now) {
                        connector.CancelReservation(connector.ReservationId());
                    }
                }

                bool meterSend = false;

                if (Now >= m_MeterValueDateTime) {
                    m_MeterValueDateTime = Now + static_cast<CDateTime>(60) / SecsPerDay; // 60 sec
                    meterSend = MeterValues();
                }

                if (!meterSend && Now >= m_HeartbeatDateTime) {
                    m_HeartbeatDateTime = Now + static_cast<CDateTime>(m_HeartbeatInterval) / SecsPerDay;
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

        COCPPClientItem *COCPPClientManager::GetItem(const int Index) const {
            return dynamic_cast<COCPPClientItem *>(inherited::GetItem(Index));
        }
        //--------------------------------------------------------------------------------------------------------------

        COCPPClientItem *COCPPClientManager::Add(const CLocation &URI) {
            return new COCPPClientItem(this, URI);
        }

    }
}

}
