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
                idTag = Value.Values("idTag");
                return *this;
            }

            AuthorizeRequest& operator<< (const CJSON& Value) {
                idTag = Value["idTag"].AsSiring();
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

                if (!Value.Values("connectorId").IsEmpty())
                    connectorId = StrToInt(Value.Values("connectorId").c_str());

                idTag = Value.Values("idTag");

                if (!Value.Values("timestamp").IsEmpty())
                    timestamp = StrToDateTimeDef(Value.Values("timestamp").c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (!Value.Values("meterStart").IsEmpty())
                    meterStart = StrToInt(Value.Values("meterStart").c_str());

                if (!Value.Values("reservationId").IsEmpty())
                    reservationId = StrToInt(Value.Values("reservationId").c_str());

                return *this;
            }

            StartTransactionRequest& operator<< (const CJSON& Value) {

                if (!Value["connectorId"].IsEmpty())
                    connectorId = Value["connectorId"].AsInteger();

                idTag = Value["idTag"].AsSiring();

                if (!Value["timestamp"].IsEmpty())
                    timestamp = StrToDateTimeDef(Value["timestamp"].AsSiring().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

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

                value = Value.Values("value");
                context = Value.Values("context");
                format = Value.Values("format");
                measurand = Value.Values("measurand");
                phase = Value.Values("phase");
                location = Value.Values("location");
                unit = Value.Values("unit");

                return *this;
            }

            SampledValue& operator<< (const CJSON& Value) {

                value = Value["value"].AsSiring();
                context = Value["context"].AsSiring();
                format = Value["format"].AsSiring();
                measurand = Value["measurand"].AsSiring();
                phase = Value["phase"].AsSiring();
                location = Value["location"].AsSiring();
                unit = Value["unit"].AsSiring();

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

                if (!Value.Values("transactionId").IsEmpty())
                    transactionId = StrToInt(Value.Values("transactionId").c_str());

                idTag = Value.Values("idTag");

                if (!Value.Values("timestamp").IsEmpty())
                    timestamp = StrToDateTimeDef(Value.Values("timestamp").c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (!Value.Values("meterStop").IsEmpty())
                    meterStop = StrToInt(Value.Values("meterStop").c_str());

                reason = Value.Values("reason");

                if (!Value.Values("transactionData").IsEmpty())
                    XMLToMeterValue(Value.Values("transactionData"), transactionData);

                return *this;
            }

            StopTransactionRequest& operator<< (const CJSON& Value) {

                if (!Value["transactionId"].IsEmpty())
                    transactionId = Value["transactionId"].AsInteger();

                idTag = Value["idTag"].AsSiring();

                if (!Value["timestamp"].IsEmpty())
                    timestamp = StrToDateTimeDef(Value["timestamp"].AsSiring().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (!Value["meterStop"].IsEmpty())
                    meterStop = Value["meterStop"].AsInteger();

                reason = Value["reason"].AsSiring();

                if (!Value["transactionData"].IsEmpty())
                    XMLToMeterValue(Value["transactionData"].AsSiring(), transactionData);

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

                iccid = Value.Values("iccid");
                chargePointVendor = Value.Values("chargePointVendor");
                chargePointModel = Value.Values("chargePointModel");
                chargePointSerialNumber = Value.Values("chargePointSerialNumber");
                chargeBoxSerialNumber = Value.Values("chargeBoxSerialNumber");
                firmwareVersion = Value.Values("firmwareVersion");
                meterType = Value.Values("meterType");
                meterSerialNumber = Value.Values("meterSerialNumber");

                return *this;
            }

            BootNotificationRequest& operator<< (const CJSON& Value) {

                iccid = Value["iccid"].AsSiring();
                chargePointVendor = Value["chargePointVendor"].AsSiring();
                chargePointModel = Value["chargePointModel"].AsSiring();
                chargePointSerialNumber = Value["chargePointSerialNumber"].AsSiring();
                chargeBoxSerialNumber = Value["chargeBoxSerialNumber"].AsSiring();
                firmwareVersion = Value["firmwareVersion"].AsSiring();
                meterType = Value["meterType"].AsSiring();
                meterSerialNumber = Value["meterSerialNumber"].AsSiring();

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

                connectorId = StrToInt(Value.Values("connectorId").c_str());
                status = COCPPMessage::StringToChargePointStatus(Value.Values("status"));
                errorCode = COCPPMessage::StringToChargePointErrorCode(Value.Values("errorCode"));
                info = Value.Values("info");
                timestamp = StrToDateTimeDef(Value.Values("timestamp").c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");
                vendorId = Value.Values("vendorId");
                vendorErrorCode = Value.Values("vendorErrorCode");

                return *this;
            }

            StatusNotificationRequest& operator<< (const CJSON& Value) {

                connectorId = Value["connectorId"].AsInteger();
                status = COCPPMessage::StringToChargePointStatus(Value["status"].AsSiring());
                errorCode = COCPPMessage::StringToChargePointErrorCode(Value["errorCode"].AsSiring());
                info = Value["info"].AsSiring();
                timestamp = StrToDateTimeDef(Value["timestamp"].AsSiring().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");
                vendorId = Value["vendorId"].AsSiring();
                vendorErrorCode = Value["vendorErrorCode"].AsSiring();

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

                vendorId = Value.Values("vendorId");
                messageId = Value.Values("messageId");
                data = Value.Values("data");

                return *this;
            }

            DataTransferRequest& operator<< (const CJSON& Value) {

                vendorId = Value["vendorId"].AsSiring();
                messageId = Value["messageId"].AsSiring();
                data = Value["data"].AsSiring();

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

            CMessageHandler(CMessageManager *AManager, COnMessageHandlerEvent && Handler);

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

            CChargingPoint *m_Point;

            CMessageHandler *Get(int Index);
            void Set(int Index, CMessageHandler *Value);

        public:

            explicit CMessageManager(CChargingPoint *APoint): CCollection(this), m_Point(APoint) {

            }

            CMessageHandler *Add(COnMessageHandlerEvent &&Handler, const CString &Action, const CJSON &Payload);

            CMessageHandler *First() { return Get(0); };
            CMessageHandler *Last() { return Get(Count() - 1); };

            CMessageHandler *FindMessageById(const CString &Value);

            CMessageHandler *Handlers(int Index) { return Get(Index); }
            void Handlers(int Index, CMessageHandler *Value) { Set(Index, Value); }

            CMessageHandler *operator[] (int Index) override { return Handlers(Index); };

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPointManager;
        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint: public CCollectionItem {
        private:

            CHTTPServerConnection *m_Connection;

            CMessageManager *m_Messages;

            CString m_Address;
            CString m_Identity;

            int m_TransactionId;

            CAuthorizeRequest m_AuthorizeRequest;
            CStartTransactionRequest m_StartTransactionRequest;
            CStopTransactionRequest m_StopTransactionRequest;
            CBootNotificationRequest m_BootNotificationRequest;
            CStatusNotificationRequest m_StatusNotificationRequest;
            CDataTransferRequest m_DataTransferRequest;

            bool ParseSOAP(const CString &Request, CString &Response);
            bool ParseJSON(const CString &Request, CString &Response);

        public:

            explicit CChargingPoint(CHTTPServerConnection *AConnection, CChargingPointManager *AManager);

            ~CChargingPoint() override;

            void SwitchConnection(CHTTPServerConnection *Value);

            CHTTPServerConnection *Connection() { return m_Connection; };

            void Connection(CHTTPServerConnection *Value) { m_Connection = Value; };

            CMessageManager *Messages() { return m_Messages; };

            CString &Address() { return m_Address; };
            const CString &Address() const { return m_Address; };

            CString &Identity() { return m_Identity; };
            const CString &Identity() const { return m_Identity; };

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

            bool Parse(CProtocolType Protocol, const CString &Request, CString &Response);

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

    }
}

using namespace Apostol::OCPP;
}

#endif //APOSTOL_OCPP_HPP
