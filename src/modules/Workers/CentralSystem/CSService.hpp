/*++

Program name:

  OCPP Central System Service

Module Name:

  CSService.hpp

Notices:

  Module: Central System Service

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef OCPP_CENTRAL_SYSTEM_SERVICE_HPP
#define OCPP_CENTRAL_SYSTEM_SERVICE_HPP
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Workers {

        struct COperation {

            CString name;
            CString type;

            bool required = false;

            COperation() = default;

            COperation(const COperation &owner) {
                if (&owner != this) {
                    Assign(owner);
                }
            }

            COperation(const CString &name, const CString &type, bool required = true): required(required) {
                this->name = name;
                this->type = type;
            }

            explicit COperation(const CJSONValue &Value) {
                *this << Value;
            }

            void Assign(const COperation &owner) {
                name = owner.name;
                type = owner.type;
                required = owner.required;
            }

            COperation& operator= (const COperation& operation) {
                if (&operation != this) {
                    Assign(operation);
                }
                return *this;
            }

            COperation& operator<< (const CJSONValue& Value) {
                name = Value["name"].AsString();
                type = Value["type"].AsString();
                required = Value["required"].AsBoolean();

                return *this;
            }

        };
        //--------------------------------------------------------------------------------------------------------------

        typedef TList<COperation> CFields;
        typedef TPairs<CFields> COperations;

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CWebhookEndpoint {

            CLocation m_URL;
            CAuthorization m_Authorization;

            bool m_Enabled = false;

        public:

            CWebhookEndpoint() = default;

            CWebhookEndpoint(const CWebhookEndpoint &owner) {
                if (&owner != this) {
                    Assign(owner);
                }
            }

            explicit CWebhookEndpoint(const CString &url, const bool enabled = true): m_Enabled(enabled) {
                this->m_URL = url;
            }

            void Assign(const CWebhookEndpoint &owner) {
                m_URL = owner.m_URL;
                m_Enabled = owner.m_Enabled;
            }

            CAuthorization &Authorization() { return m_Authorization; }
            const CAuthorization &Authorization() const { return m_Authorization; }

            CLocation &URL() { return m_URL; }
            const CLocation &URL() const { return m_URL; }

            bool Enabled() const { return m_Enabled; }
            void Enabled(const bool Value) { m_Enabled = Value; }

        };
        //--------------------------------------------------------------------------------------------------------------

        class CCSService: public CApostolModule {
        private:

            CWebhookEndpoint m_Webhook;

            COperations m_Endpoints;
            COperations m_Operations;

            CCSChargingPointManager m_PointManager;

            void InitMethods() override;
            void InitOperations();
            void InitEndpoints();

            bool ConnectionExists(CWebSocketConnection *AConnection);

#ifdef WITH_AUTHORIZATION
            void VerifyToken(const CString &Token);
            static bool CheckAuthorizationData(const CHTTPRequest &Request, CAuthorization &Authorization);
            bool CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization);
#endif
            void DoWebSocketError(CHTTPServerConnection *AConnection, const Delphi::Exception::Exception &E);

            static void SOAPError(CHTTPServerConnection *AConnection, const CString &Code, const CString &SubCode,
                                    const CString &Reason, const CString &Message);
#ifdef WITH_POSTGRESQL
            void QueryResponse(CPQPollQuery *APollQuery);

            void ParseJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CJSONMessage &Message, const CString &Account = {});
            void ParseSOAP(CHTTPServerConnection *AConnection, const CString &Payload);

            void JSONToSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation, const CJSON &Payload);
            void SOAPToJSON(CHTTPServerConnection *AConnection, const CString &Payload);

            void SetPointConnected(const CString &Identity, bool Value, const CString &Metadata);
#endif
            void SendSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation, const CString &Payload);
            void SendJSON(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CJSONMessage &Message);

            void WebhookJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CJSONMessage &Message, const CString &Account = {});
            void WebhookSOAP(CHTTPServerConnection *AConnection, const CString &Payload) const;

            static void LogJSONMessage(const CString &Identity, const CJSONMessage &Message);

            CJSONValue ChargePointToJson(CCSChargingPoint *APoint);
            CJSON GetChargePointList();

            static int CheckError(const CJSON &Json, CString &ErrorMessage, bool RaiseIfError = false);
            static CHTTPReply::CStatusType ErrorCodeToStatus(int ErrorCode);

            void OnChargePointMessageSOAP(CObject *Sender, const CSOAPMessage &Message) const;
            void OnChargePointMessageJSON(CObject *Sender, const CJSONMessage &Message) const;

        protected:

            void DoCentralSystem(CHTTPServerConnection *AConnection, const CString &Token, const CString &Endpoint);
            void DoChargePoint(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Operation);

            void DoChargePointList(CHTTPServerConnection *AConnection);

            void DoWebSocket(CHTTPServerConnection *AConnection);
            void DoAPI(CHTTPServerConnection *AConnection);

            void DoGet(CHTTPServerConnection *AConnection) override;
            void DoPost(CHTTPServerConnection *AConnection);

            void DoSOAP(CHTTPServerConnection *AConnection);
            int DoOCPP(CHTTPServerConnection *AConnection);

            void DoPointConnected(CCSChargingPoint *APoint, bool Value);
            void DoPointDisconnected(CObject *Sender);
#ifdef WITH_POSTGRESQL
            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) override;
#endif
        public:

            explicit CCSService(CModuleProcess *AProcess);

            ~CCSService() override = default;

            static CCSService *CreateModule(CModuleProcess *AProcess) {
                return new CCSService(AProcess);
            }

            void Initialization(CModuleProcess *AProcess) override;

            bool Execute(CHTTPServerConnection *AConnection) override;

            bool Enabled() override;

            static void SendError(CHTTPServerConnection *AConnection, CHTTPReply::CStatusType ErrorCode, const CString &Message);

        };
    }
}

using namespace Apostol::Workers;
}
#endif //OCPP_CENTRAL_SYSTEM_SERVICE_HPP
