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

    namespace CSService {

        typedef struct CDataBase {
            CString Username;
            CString Password;
            CString Session;
        } CDataBase;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct CMessage {
            CStringPairs Headers;
            CStringPairs Values;
            CString Notification;
        } CMessage;

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CSOAPProtocol {
        public:

            static void Request(const CString &xmlString, CMessage &Message);
            static void Response(const CMessage &Message, CString &xmlString);

            static void PrepareResponse(const CMessage &Request, CMessage &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        enum CChargePointStatus { cpsUnknown = -1, cpsAvailable, cpsOccupied, cpsFaulted, cpsUnavailable, cpsReserved };

        enum CChargePointErrorCode { cpeUnknown = -1, cpeConnectorLockFailure, cpeHighTemperature, cpeMode3Error, cpeNoError,
                cpePowerMeterFailure, cpePowerSwitchFailure, cpeReaderFailure, cpeResetFailure, cpeGroundFailure,
                cpeOverCurrentFailure, cpeUnderVoltage, cpeWeakSignal, cpeOtherError };

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint;
        class CChargingPointManager;
        //--------------------------------------------------------------------------------------------------------------

        class CSOAPMessage {
        public:

            static CChargePointStatus StringToChargePointStatus(const CString& Value);
            static CString ChargePointStatusToString(CChargePointStatus Value);

            static CChargePointErrorCode StringToChargePointErrorCode(const CString& Value);
            static CString ChargePointErrorCodeToString(CChargePointErrorCode Value);

            static void Parse(CChargingPoint *APoint, const CMessage &Request, CMessage &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        typedef struct AuthorizeRequest {
            CString idTag;
        } CAuthorizeRequest, *PAuthorizeRequest;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct StartTransactionRequest {
            int connectorId = 0;
            CString idTag;
            CDateTime timestamp = 0;
            int meterStart = 0;
            int reservationId = 0;
        } CStartTransactionRequest, *PStartTransactionRequest;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct SampledValue {
            CString value;
            CString context;
            CString format;
            CString measurand;
            CString phase;
            CString location;
            CString unit;
        } CSampledValue, *PSampledValue;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct MeterValue {
            CDateTime timestamp = 0;
            CSampledValue sampledValue;
        } CMeterValue, *PMeterValue;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct StopTransactionRequest {
            int transactionId = 0;
            CString idTag;
            CDateTime timestamp = 0;
            int meterStop = 0;
            CString reason;
            CMeterValue transactionData;
        } CStopTransactionRequest, *PStopTransactionRequest;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct BootNotificationRequest {
            CString imsi;
            CString iccid;
            CString chargePointVendor;
            CString chargePointModel;
            CString chargePointSerialNumber;
            CString chargeBoxSerialNumber;
            CString firmwareVersion;
            CString meterType;
            CString meterSerialNumber;
        } CBootNotificationRequest, *PBootNotificationRequest;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct StatusNotificationRequest {
            int connectorId = 0;
            CChargePointStatus status = cpsUnknown;
            CChargePointErrorCode errorCode = cpeUnknown;
            CString info;
            CDateTime timestamp = 0;
            CString vendorId;
            CString vendorErrorCode;
        } CStatusNotificationRequest, *PStatusNotificationRequest;
        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint: public CCollectionItem {
        private:

            CString m_Address;
            CString m_Identity;

            int m_TransactionId;

            CAuthorizeRequest m_AuthorizeRequest;
            CStartTransactionRequest m_StartTransactionRequest;
            CStopTransactionRequest m_StopTransactionRequest;
            CBootNotificationRequest m_BootNotificationRequest;
            CStatusNotificationRequest m_StatusNotificationRequest;

            CHTTPServerConnection *m_Connection;

        public:

            explicit CChargingPoint(CHTTPServerConnection *AConnection, CChargingPointManager *AManager);

            ~CChargingPoint() override;

            CHTTPServerConnection *Connection() { return m_Connection; };

            void Connection(CHTTPServerConnection *Value) { m_Connection = Value; };

            CString &Address() { return m_Address; };
            const CString &Address() const { return m_Address; };

            CString &Identity() { return m_Identity; };
            const CString &Identity() const { return m_Identity; };

            static void StringToMeterValue(const CString &String, CMeterValue& MeterValue);

            void Authorize(const CMessage &Request, CMessage &Response);
            void StartTransaction(const CMessage &Request, CMessage &Response);
            void StopTransaction(const CMessage &Request, CMessage &Response);
            void BootNotification(const CMessage &Request, CMessage &Response);
            void StatusNotification(const CMessage &Request, CMessage &Response);
            void Heartbeat(const CMessage &Request, CMessage &Response);

            void Parse(const CString &Request, CString &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPointManager -------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPointManager: public CCollection {
            typedef CCollection inherited;

        private:

            CChargingPoint *Get(int Index);
            void Set(int Index, CChargingPoint *Value);

        public:

            CChargingPointManager(): CCollection(this) {

            }

            CChargingPoint *Add(CHTTPServerConnection *AConnection);

            CChargingPoint *First() { return Get(0); };
            CChargingPoint *Last() { return Get(Count() - 1); };

            CChargingPoint *FindPointByIdentity(const CString &Value);
            CChargingPoint *FindPointByConnection(CHTTPServerConnection *Value);

            CChargingPoint *Points(int Index) { return Get(Index); }
            void Points(int Index, CChargingPoint *Value) { Set(Index, Value); }

            CChargingPoint *operator[] (int Index) override { return Points(Index); };

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSService ------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCSService: public CApostolModule {
        private:

            CChargingPointManager *m_CPManager;

            static void PQResultToJson(CPQResult *Result, CString& Json);

            void QueryToJson(CPQPollQuery *Query, CString& Json);

            bool QueryStart(CHTTPServerConnection *AConnection, const CStringList& SQL);

            static void DebugRequest(CRequest *ARequest);

            static void DebugReply(CReply *AReply);

        protected:

            void DoGet(CHTTPServerConnection *AConnection);
            void DoPost(CHTTPServerConnection *AConnection);

            void DoPointDisconnected(CObject *Sender);

            void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) override;
            void DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) override;

            static void ExceptionToJson(int ErrorCode, Delphi::Exception::Exception *AException, CString& Json);

            bool APIRun(CHTTPServerConnection *AConnection, const CString &Route, const CString &jsonString, const CDataBase &DataBase);

        public:

            explicit CCSService(CModuleManager *AManager);

            ~CCSService() override;

            static class CCSService *CreateModule(CModuleManager *AManager) {
                return new CCSService(AManager);
            }

            void InitMethods() override;

            void BeforeExecute(Pointer Data) override;
            void AfterExecute(Pointer Data) override;

            void Execute(CHTTPServerConnection *AConnection) override;

            bool CheckUserAgent(const CString& Value) override;

        };
    }
}

using namespace Apostol::CSService;
}
#endif //APOSTOL_WEBSERVICE_HPP
