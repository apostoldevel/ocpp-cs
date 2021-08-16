/*++

Library name:

  apostol-core

Module Name:

  OCPP.hpp

Notices:

  Open Charge Point Protocol (OCPP)

  OCPP-S 1.5
  OCPP-J 1.6

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_OCPP_HPP
#define APOSTOL_OCPP_HPP

extern "C++" {

namespace Apostol {

    namespace OCPP {

        enum CProtocolType { ptSOAP, ptJSON };
        //--------------------------------------------------------------------------------------------------------------

        typedef struct CSOAPMessage {
            CStringPairs Headers;
            CStringPairs Values;
            CString Notification;
        } CSOAPMessage;
        //--------------------------------------------------------------------------------------------------------------

        enum CMessageType { mtCall = 0, mtCallResult, mtCallError };

        typedef struct CJSONMessage {
            CMessageType MessageTypeId;
            CString UniqueId;
            CString Action;
            CString ErrorCode;
            CString ErrorDescription;
            CJSON Payload;

            size_t Size() const {
                return UniqueId.Size() + Action.Size() + ErrorCode.Size() + ErrorDescription.Size();
            }

        } CJSONMessage;

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CSOAPProtocol {
        public:

            static void Request(const CString &xmlString, CSOAPMessage &Message);
            static void Response(const CSOAPMessage &Message, CString &xmlString);

            static void PrepareResponse(const CSOAPMessage &Request, CSOAPMessage &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CJSONProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        enum CJSONParserState { psBegin, psMessageTyeId, psUniqueId, psAction, psErrorCode, psErrorDescription, psPayloadBegin,
                psPayloadObject, psPayloadArray, psEnd };

        class CJSONProtocol {
        public:
            static void ExceptionToJson(int ErrorCode, const std::exception &e, CJSON& Json);

            static bool Request(const CString &String, CJSONMessage &Message);
            static void Response(const CJSONMessage &Message, CString &String);

            static void PrepareResponse(const CJSONMessage &Request, CJSONMessage &Response);

            static void Call(const CString &UniqueId, const CString &Action, const CJSON &Payload, CString &Result);
            static void CallResult(const CString &UniqueId, const CJSON &Payload, CString &Result);
            static void CallError(const CString &UniqueId, const CString &ErrorCode, const CString &ErrorDescription, const CJSON &Payload, CString &Result);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        enum CChargePointStatus { cpsUnknown = -1, cpsAvailable, cpsOccupied, cpsFaulted, cpsUnavailable, cpsReserved };

        enum CChargePointErrorCode { cpeUnknown = -1, cpeConnectorLockFailure, cpeHighTemperature, cpeMode3Error, cpeNoError,
            cpePowerMeterFailure, cpePowerSwitchFailure, cpeReaderFailure, cpeResetFailure, cpeGroundFailure,
            cpeOverCurrentFailure, cpeUnderVoltage, cpeWeakSignal, cpeOtherError };

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint;
        //--------------------------------------------------------------------------------------------------------------

        class COCPPMessage {
        public:

            static CChargePointStatus StringToChargePointStatus(const CString& Value);
            static CString ChargePointStatusToString(CChargePointStatus Value);

            static CChargePointErrorCode StringToChargePointErrorCode(const CString& Value);
            static CString ChargePointErrorCodeToString(CChargePointErrorCode Value);

            static bool Parse(CChargingPoint *APoint, const CSOAPMessage &Request, CSOAPMessage &Response);
            static bool Parse(CChargingPoint *APoint, const CJSONMessage &Request, CJSONMessage &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        typedef struct AuthorizeRequest {
            CString idTag;

            AuthorizeRequest& operator<< (const CStringPairs& Value) {
                idTag = Value["idTag"];
                return *this;
            }

            AuthorizeRequest& operator<< (const CJSON& Value) {
                idTag = Value["idTag"].AsString();
                return *this;
            }

        } CAuthorizeRequest, *PAuthorizeRequest;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct StartTransactionRequest {

            int connectorId = 0;
            CString idTag;
            CDateTime timestamp = 0;
            int meterStart = 0;
            int reservationId = 0;

            StartTransactionRequest& operator<< (const CStringPairs& Value) {

                if (!Value["connectorId"].IsEmpty())
                    connectorId = StrToInt(Value["connectorId"].c_str());

                idTag = Value["idTag"];

                if (!Value["timestamp"].IsEmpty())
                    timestamp = StrToDateTimeDef(Value["timestamp"].c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (!Value["meterStart"].IsEmpty())
                    meterStart = StrToInt(Value["meterStart"].c_str());

                if (!Value["reservationId"].IsEmpty())
                    reservationId = StrToInt(Value["reservationId"].c_str());

                return *this;
            }

            StartTransactionRequest& operator<< (const CJSON& Value) {

                if (!Value["connectorId"].IsEmpty())
                    connectorId = Value["connectorId"].AsInteger();

                idTag = Value["idTag"].AsString();

                if (!Value["timestamp"].IsEmpty())
                    timestamp = StrToDateTimeDef(Value["timestamp"].AsString().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (!Value["meterStart"].IsEmpty())
                    meterStart = Value["meterStart"].AsInteger();

                if (!Value["reservationId"].IsEmpty())
                    reservationId = Value["reservationId"].AsInteger();

                return *this;
            }

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

            SampledValue& operator<< (const CStringPairs& Value) {

                value = Value["value"];
                context = Value["context"];
                format = Value["format"];
                measurand = Value["measurand"];
                phase = Value["phase"];
                location = Value["location"];
                unit = Value["unit"];

                return *this;
            }

            SampledValue& operator<< (const CJSON& Value) {

                value = Value["value"].AsString();
                context = Value["context"].AsString();
                format = Value["format"].AsString();
                measurand = Value["measurand"].AsString();
                phase = Value["phase"].AsString();
                location = Value["location"].AsString();
                unit = Value["unit"].AsString();

                return *this;
            }

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

            static void XMLToMeterValue(const CString &xmlString, CMeterValue& MeterValue);

            StopTransactionRequest& operator<< (const CStringPairs& Value) {

                if (!Value["transactionId"].IsEmpty())
                    transactionId = StrToInt(Value["transactionId"].c_str());

                idTag = Value["idTag"];

                if (!Value["timestamp"].IsEmpty())
                    timestamp = StrToDateTimeDef(Value["timestamp"].c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (!Value["meterStop"].IsEmpty())
                    meterStop = StrToInt(Value["meterStop"].c_str());

                reason = Value["reason"];

                if (!Value["transactionData"].IsEmpty())
                    XMLToMeterValue(Value["transactionData"], transactionData);

                return *this;
            }

            StopTransactionRequest& operator<< (const CJSON& Value) {

                if (!Value["transactionId"].IsEmpty())
                    transactionId = Value["transactionId"].AsInteger();

                idTag = Value["idTag"].AsString();

                if (!Value["timestamp"].IsEmpty())
                    timestamp = StrToDateTimeDef(Value["timestamp"].AsString().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (!Value["meterStop"].IsEmpty())
                    meterStop = Value["meterStop"].AsInteger();

                reason = Value["reason"].AsString();

                if (!Value["transactionData"].IsEmpty())
                    XMLToMeterValue(Value["transactionData"].AsString(), transactionData);

                return *this;
            }

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

            BootNotificationRequest& operator<< (const CStringPairs& Value) {

                imsi = Value["imsi"];
                iccid = Value["iccid"];
                chargePointVendor = Value["chargePointVendor"];
                chargePointModel = Value["chargePointModel"];
                chargePointSerialNumber = Value["chargePointSerialNumber"];
                chargeBoxSerialNumber = Value["chargeBoxSerialNumber"];
                firmwareVersion = Value["firmwareVersion"];
                meterType = Value["meterType"];
                meterSerialNumber = Value["meterSerialNumber"];

                return *this;
            }

            BootNotificationRequest& operator<< (const CJSON& Value) {

                imsi = Value["imsi"].AsString();
                iccid = Value["iccid"].AsString();
                chargePointVendor = Value["chargePointVendor"].AsString();
                chargePointModel = Value["chargePointModel"].AsString();
                chargePointSerialNumber = Value["chargePointSerialNumber"].AsString();
                chargeBoxSerialNumber = Value["chargeBoxSerialNumber"].AsString();
                firmwareVersion = Value["firmwareVersion"].AsString();
                meterType = Value["meterType"].AsString();
                meterSerialNumber = Value["meterSerialNumber"].AsString();

                return *this;
            }

            const BootNotificationRequest& operator>> (CJSONValue& Value) const {

                if (Value.IsObject()) {
                    CJSONObject &Object = Value.Object();

                    Object.AddPair("iccid", iccid);
                    Object.AddPair("chargePointVendor", chargePointVendor);
                    Object.AddPair("chargePointModel", chargePointModel);
                    Object.AddPair("chargePointSerialNumber", chargePointSerialNumber);
                    Object.AddPair("firmwareVersion", firmwareVersion);
                    Object.AddPair("meterType", meterType);
                    Object.AddPair("meterSerialNumber", meterSerialNumber);
                }

                return *this;
            }

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

            StatusNotificationRequest& operator<< (const CStringPairs& Value) {

                connectorId = StrToInt(Value["connectorId"].c_str());
                status = COCPPMessage::StringToChargePointStatus(Value["status"]);
                errorCode = COCPPMessage::StringToChargePointErrorCode(Value["errorCode"]);
                info = Value["info"];
                timestamp = StrToDateTimeDef(Value["timestamp"].c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");
                vendorId = Value["vendorId"];
                vendorErrorCode = Value["vendorErrorCode"];

                return *this;
            }

            StatusNotificationRequest& operator<< (const CJSON& Value) {

                connectorId = Value["connectorId"].AsInteger();
                status = COCPPMessage::StringToChargePointStatus(Value["status"].AsString());
                errorCode = COCPPMessage::StringToChargePointErrorCode(Value["errorCode"].AsString());
                info = Value["info"].AsString();
                timestamp = StrToDateTimeDef(Value["timestamp"].AsString().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");
                vendorId = Value["vendorId"].AsString();
                vendorErrorCode = Value["vendorErrorCode"].AsString();

                return *this;
            }

            const StatusNotificationRequest& operator>> (CJSONValue& Value) const {

                if (Value.IsObject()) {
                    TCHAR szDate[25] = {0};
                    CJSONObject &Object = Value.Object();

                    Object.AddPair("connectorId", connectorId);
                    Object.AddPair("status", COCPPMessage::ChargePointStatusToString(status));
                    Object.AddPair("errorCode", COCPPMessage::ChargePointErrorCodeToString(errorCode));
                    Object.AddPair("info", info);
                    Object.AddPair("timestamp", DateTimeToStr(timestamp, szDate, sizeof(szDate)));
                    Object.AddPair("vendorId", vendorId);
                    Object.AddPair("vendorErrorCode", vendorErrorCode);
                }

                return *this;
            }

        } CStatusNotificationRequest, *PStatusNotificationRequest;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct DataTransferRequest {

            CString vendorId;
            CString messageId;
            CString data;

            DataTransferRequest& operator<< (const CStringPairs& Value) {

                vendorId = Value["vendorId"];
                messageId = Value["messageId"];
                data = Value["data"];

                return *this;
            }

            DataTransferRequest& operator<< (const CJSON& Value) {

                vendorId = Value["vendorId"].AsString();
                messageId = Value["messageId"].AsString();
                data = Value["data"].AsString();

                return *this;
            }

        } CDataTransferRequest;

        //--------------------------------------------------------------------------------------------------------------

        //-- CMessageHandler -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CMessageManager;
        class CMessageHandler;
        //--------------------------------------------------------------------------------------------------------------

        typedef std::function<void (CMessageHandler *Handler, CHTTPServerConnection *Connection)> COnMessageHandlerEvent;
        //--------------------------------------------------------------------------------------------------------------

        class CMessageHandler: public CCollectionItem {
        private:

            CString m_UniqueId;
            CString m_Action;

            COnMessageHandlerEvent m_Handler;

        public:

            CMessageHandler(OCPP::CMessageManager *AManager, COnMessageHandlerEvent && Handler);

            const CString &UniqueId() const { return m_UniqueId; }

            CString &Action() { return m_Action; }
            const CString &Action() const { return m_Action; }

            void Handler(CHTTPServerConnection *AConnection);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CMessageManager -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CMessageManager: public CCollection {
            typedef CCollection inherited;

        private:

            CChargingPoint *m_pPoint;

            CMessageHandler *Get(int Index) const;
            void Set(int Index, CMessageHandler *Value);

        public:

            explicit CMessageManager(CChargingPoint *APoint): CCollection(this), m_pPoint(APoint) {

            }

            void SendAll(CHTTPServerConnection *AConnection);

            CMessageHandler *Add(COnMessageHandlerEvent &&Handler, const CString &Action, const CJSON &Payload);

            CMessageHandler *First() { return Get(0); };
            CMessageHandler *Last() { return Get(Count() - 1); };

            CMessageHandler *FindMessageById(const CString &Value);

            CMessageHandler *Handlers(int Index) const { return Get(Index); }
            void Handlers(int Index, CMessageHandler *Value) { Set(Index, Value); }

            CMessageHandler *operator[] (int Index) const override { return Handlers(Index); };

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPointManager;
        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint: public CCollectionItem {
        private:

            CProtocolType m_ProtocolType;

            CHTTPServerConnection *m_pConnection;

            CMessageManager *m_pMessages;

            CString m_Address;
            CString m_Identity;

            int m_TransactionId;
            int m_UpdateCount;

            bool m_bUpdateConnected;

            CAuthorizeRequest m_AuthorizeRequest;
            CStartTransactionRequest m_StartTransactionRequest;
            CStopTransactionRequest m_StopTransactionRequest;
            CBootNotificationRequest m_BootNotificationRequest;
            CStatusNotificationRequest m_StatusNotificationRequest;
            CDataTransferRequest m_DataTransferRequest;

            void AddToConnection(CHTTPServerConnection *AConnection);
            void DeleteFromConnection(CHTTPServerConnection *AConnection);

            bool ParseSOAP(const CString &Request, CString &Response);
            bool ParseJSON(const CString &Request, CString &Response);

            void SetProtocolType(CProtocolType Value);
            void SetUpdateConnected(bool Value);

        public:

            explicit CChargingPoint(CHTTPServerConnection *AConnection, CChargingPointManager *AManager);

            ~CChargingPoint() override;

            void SwitchConnection(CHTTPServerConnection *Value);

            void BeginUpdate() { m_UpdateCount++; }
            void EndUpdate() { m_UpdateCount--; }

            int UpdateCount() const { return m_UpdateCount; }

            CHTTPServerConnection *Connection() { return m_pConnection; };

            void Connection(CHTTPServerConnection *Value) { m_pConnection = Value; };

            CMessageManager *Messages() { return m_pMessages; };

            CString &Address() { return m_Address; };
            const CString &Address() const { return m_Address; };

            CString &Identity() { return m_Identity; };
            const CString &Identity() const { return m_Identity; };

            void UpdateConnected(bool Value) { SetUpdateConnected(Value); };
            bool UpdateConnected() const { return m_bUpdateConnected; };

            void ProtocolType(CProtocolType Value) { SetProtocolType(Value); };
            CProtocolType ProtocolType() const { return m_ProtocolType; };

            const CAuthorizeRequest &AuthorizeRequest() const { return m_AuthorizeRequest; }
            const CStartTransactionRequest &StartTransactionRequest() const { return m_StartTransactionRequest; }
            const CStopTransactionRequest &StopTransactionRequest() const { return m_StopTransactionRequest; }
            const CBootNotificationRequest &BootNotificationRequest() const { return m_BootNotificationRequest; }
            const CStatusNotificationRequest &StatusNotificationRequest() const { return m_StatusNotificationRequest; }
            const CDataTransferRequest &DataTransferRequest() const { return m_DataTransferRequest; }

            void Authorize(const CSOAPMessage &Request, CSOAPMessage &Response);
            void Authorize(const CJSONMessage &Request, CJSONMessage &Response);

            void StartTransaction(const CSOAPMessage &Request, CSOAPMessage &Response);
            void StartTransaction(const CJSONMessage &Request, CJSONMessage &Response);

            void StopTransaction(const CSOAPMessage &Request, CSOAPMessage &Response);
            void StopTransaction(const CJSONMessage &Request, CJSONMessage &Response);

            void BootNotification(const CSOAPMessage &Request, CSOAPMessage &Response);
            void BootNotification(const CJSONMessage &Request, CJSONMessage &Response);

            void StatusNotification(const CSOAPMessage &Request, CSOAPMessage &Response);
            void StatusNotification(const CJSONMessage &Request, CJSONMessage &Response);

            void DataTransfer(const CSOAPMessage &Request, CSOAPMessage &Response);
            void DataTransfer(const CJSONMessage &Request, CJSONMessage &Response);

            void Heartbeat(const CSOAPMessage &Request, CSOAPMessage &Response);
            void Heartbeat(const CJSONMessage &Request, CJSONMessage &Response);

            bool Parse(const CString &Request, CString &Response);

            static CChargingPoint *FindOfConnection(CHTTPServerConnection *AConnection);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPointManager -------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPointManager: public CCollection {
            typedef CCollection inherited;

        private:

            CChargingPoint *Get(int Index) const;
            void Set(int Index, CChargingPoint *Value);

        public:

            CChargingPointManager(): CCollection(this) {

            }

            CChargingPoint *Add(CHTTPServerConnection *AConnection);

            CChargingPoint *First() { return Get(0); };
            CChargingPoint *Last() { return Get(Count() - 1); };

            CChargingPoint *FindPointByIdentity(const CString &Value);
            CChargingPoint *FindPointByConnection(CHTTPServerConnection *Value);

            CChargingPoint *Points(int Index) const { return Get(Index); }
            void Points(int Index, CChargingPoint *Value) { Set(Index, Value); }

            CChargingPoint *operator[] (int Index) const override { return Points(Index); };

        };

    }
}

using namespace Apostol::OCPP;
}

#endif //APOSTOL_OCPP_HPP
