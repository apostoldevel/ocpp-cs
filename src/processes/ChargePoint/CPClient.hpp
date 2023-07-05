/*++

Program name:

  OCPP Charge Point Service

Module Name:

  CPClient.hpp

Notices:

  Charge Point Client

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef OCPP_CHARGE_POINT_CLIENT_HPP
#define OCPP_CHARGE_POINT_CLIENT_HPP
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace ChargePoint {

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPConnection -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class COCPPClient;
        //--------------------------------------------------------------------------------------------------------------

        class COCPPConnection: public CHTTPClientConnection {
            typedef CHTTPClientConnection inherited;

        private:

            CString m_Identity {};
            CString m_Action {};

        public:

            explicit COCPPConnection(CPollSocketClient *AClient) : CHTTPClientConnection(AClient) {
                TimeOut(INFINITE);
                CloseConnection(false);
            }

            ~COCPPConnection() override = default;

            COCPPClient *OCPPClient() { return (COCPPClient *) CHTTPClientConnection::Client(); };

            CString &Identity() { return m_Identity; };
            const CString &Identity() const { return m_Identity; };

            CString &Action() { return m_Action; };
            const CString &Action() const { return m_Action; };

        }; // COCPPConnection

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPClient -----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        typedef std::function<void (const COCPPClient &Client)> COnOCPPClientEvent;
        //--------------------------------------------------------------------------------------------------------------

        class COCPPClient: public CGlobalComponent, public CHTTPClient, public CChargingPoint {
        private:

            CLocation m_URI;

            CString m_Key;

            CEPollTimer *m_pTimer;

            CStringList m_Actions {true};

            uint32_t m_ErrorCount;

            int m_TimerInterval;
            int m_HeartbeatInterval;

            CDateTime m_PingDateTime;
            CDateTime m_HeartbeatDateTime;
            CDateTime m_MeterValueDateTime;
            CDateTime m_BootNotificationDateTime;

            void SetTimerInterval(int Value);
            void UpdateTimer();

        protected:

            void IncErrorCount();
            void ClearErrorCount() { m_ErrorCount = 0; };

            void DoHandshake(COCPPConnection *AConnection);

            void DoWebSocket(CHTTPClientConnection *AConnection);
            void DoHTTP(CHTTPClientConnection *AConnection);

            void DoDebugWait(CObject *Sender);
            void DoDebugRequest(CObject *Sender);
            void DoDebugReply(CObject *Sender);

            void DoConnectStart(CIOHandlerSocket *AIOHandler, CPollEventHandler *AHandler) override;
            bool DoExecute(CTCPConnection *AConnection) override;

            void DoConnect(CPollEventHandler *AHandler) override;
            void DoTimer(CPollEventHandler *AHandler) override;

            void DoHeartbeat(CDateTime Now);

        public:

            COCPPClient();

            explicit COCPPClient(const CLocation &URI);

            ~COCPPClient() override;

            void Initialize() override;

            uint32_t ErrorCount() const { return m_ErrorCount; }

            CString &Key() { return m_Key; }
            const CString &Key() const { return m_Key; }

            CLocation &URI() { return m_URI; }
            const CLocation &URI() const { return m_URI; }

            CStringList &Actions() { return m_Actions; }
            const CStringList &Actions() const { return m_Actions; }

            int TimerInterval() const { return m_TimerInterval; }
            void TimerInterval(int Value) { SetTimerInterval(Value); }

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPClientItem -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class COCPPClientManager;
        //--------------------------------------------------------------------------------------------------------------

        class COCPPClientItem: public CCollectionItem, public COCPPClient {
        public:

            explicit COCPPClientItem(COCPPClientManager *AManager);

            explicit COCPPClientItem(COCPPClientManager *AManager, const CLocation &URI);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPClientManager ----------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class COCPPClientManager: public CCollection {
            typedef CCollection inherited;

        protected:

            COCPPClientItem *GetItem(int Index) const override;

        public:

            COCPPClientManager(): CCollection(this) {

            };

            ~COCPPClientManager() override = default;

            COCPPClientItem *Add(const CLocation &URI);

            COCPPClientItem *Items(int Index) const override { return GetItem(Index); };

            COCPPClientItem *operator[] (int Index) const override { return Items(Index); };

        };
        
    }
}

using namespace Apostol::ChargePoint;
}

#endif //OCPP_CHARGE_POINT_CLIENT_HPP
