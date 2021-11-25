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

            COperation(const CString &name, const CString &type, bool required = true) {
                this->name = name;
                this->type = type;
                this->required = required;
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

        class CCSService: public CApostolModule {
        private:

            COperations m_Operations;

            CCSChargingPointManager m_PointManager;

            void InitMethods() override;
            void InitOperations();

#ifdef WITH_AUTHORIZATION
            void VerifyToken(const CString &Token);
            static bool CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization);
            bool CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization);
#endif
            static void DoWebSocketError(CHTTPServerConnection *AConnection, const Delphi::Exception::Exception &E);

            static void SOAPError(CHTTPServerConnection *AConnection, const CString &Code, const CString &SubCode,
                                    const CString &Reason, const CString &Message);
#ifdef WITH_POSTGRESQL
            void ParseJSON(CHTTPServerConnection *AConnection, const CString &Identity, const CJSONMessage &Message);
            void ParseSOAP(CHTTPServerConnection *AConnection, const CString &Payload);

            void JSONToSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation, const CJSON &Payload);
            void SOAPToJSON(CHTTPServerConnection *AConnection, const CString &Payload);

            void SetPointConnected(CCSChargingPoint *APoint, bool Value);
#endif
            void SendSOAP(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CString &Operation, const CString &Payload);
            static void SendJSON(CHTTPServerConnection *AConnection, CCSChargingPoint *APoint, const CJSONMessage &Message);

            static void LogJSONMessage(const CString &Identity, const CJSONMessage &Message);

            void OnChargePointMessageSOAP(CObject *Sender, const CSOAPMessage &Message);
            void OnChargePointMessageJSON(CObject *Sender, const CJSONMessage &Message);

        protected:

            void DoChargePoint(CHTTPServerConnection *AConnection, const CString &Identity, const CString &Operation);
            void DoChargePointList(CHTTPServerConnection *AConnection);

            void DoWebSocket(CHTTPServerConnection *AConnection);
            void DoAPI(CHTTPServerConnection *AConnection);

            void DoGet(CHTTPServerConnection *AConnection) override;
            void DoPost(CHTTPServerConnection *AConnection);

            void DoSOAP(CHTTPServerConnection *AConnection);
            void DoOCPP(CHTTPServerConnection *AConnection);

            void DoPointConnected(CCSChargingPoint *APoint);
            void DoPointDisconnected(CObject *Sender);
#ifdef WITH_POSTGRESQL
            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, const Delphi::Exception::Exception &E) override;
#endif
        public:

            explicit CCSService(CModuleProcess *AProcess);

            ~CCSService() override = default;

            static class CCSService *CreateModule(CModuleProcess *AProcess) {
                return new CCSService(AProcess);
            }

            void Initialization(CModuleProcess *AProcess) override;

            void Heartbeat() override;
            bool Execute(CHTTPServerConnection *AConnection) override;

            bool Enabled() override;

        };
    }
}

using namespace Apostol::Workers;
}
#endif //OCPP_CENTRAL_SYSTEM_SERVICE_HPP
