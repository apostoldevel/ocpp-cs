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

#ifndef OCPP_PROTOCOL_HPP
#define OCPP_PROTOCOL_HPP
//----------------------------------------------------------------------------------------------------------------------

#define OCPP_PROTOCOL_ERROR_MESSAGE "Sender's message does not comply with protocol specification"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace ChargePoint {

        CString GenUniqueId();
        //--------------------------------------------------------------------------------------------------------------

        enum CProtocolType { ptSOAP, ptJSON };
        enum CMessageType { mtCall = 0, mtCallResult, mtCallError };
        //--------------------------------------------------------------------------------------------------------------

        struct CSOAPMessage {
            CStringPairs Headers;
            CStringPairs Values;
            CString Notification;

            CSOAPMessage() = default;

            CSOAPMessage(const CSOAPMessage &Message) {
                if (&Message != this) {
                    Assign(Message);
                }
            }

            void Assign(const CSOAPMessage &Message) {
                Headers = Message.Headers;
                Values = Message.Values;
                Notification = Message.Notification;
            }

            CSOAPMessage& operator= (const CSOAPMessage& Value) {
                Assign(Value);
                return *this;
            }

        };
        //--------------------------------------------------------------------------------------------------------------

        struct CJSONMessage {
            CMessageType MessageTypeId = mtCall;
            CString UniqueId;
            CString Action;
            CString ErrorCode;
            CString ErrorDescription;
            CJSON Payload;

            CJSONMessage() = default;

            CJSONMessage(const CJSONMessage &Message): CJSONMessage() {
                if (&Message != this) {
                    Assign(Message);
                }
            }

            void Assign(const CJSONMessage &Message) {
                MessageTypeId = Message.MessageTypeId;
                UniqueId = Message.UniqueId;
                Action = Message.Action;
                ErrorCode = Message.ErrorCode;
                ErrorDescription = Message.ErrorDescription;
                Payload = Message.Payload;
            }

            size_t Size() const {
                return UniqueId.Size() + Action.Size() + ErrorCode.Size() + ErrorDescription.Size();
            }

            CJSONMessage& operator= (const CJSONMessage& Value) {
                Assign(Value);
                return *this;
            }

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CSOAPProtocol {
        public:

            static void JSONToSOAP(const CString &Identity, const CString &Action, const CString &MessageId, const CString &From, const CString &To, const CJSON &Json, CString &xmlString);
            static void SOAPToJSON(const CString &xmlString, CJSON &Json);

            static void Request(const CString &xmlString, CSOAPMessage &Message);
            static void Response(const CSOAPMessage &Message, CString &xmlString);

            static void PrepareResponse(const CSOAPMessage &Request, CSOAPMessage &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CJSONProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        enum CJSONParserState { psBegin, psMessageTyeId, psUniqueId, psAction, psErrorCode, psErrorDescription, psPayloadBegin,
                psPayloadObject, psPayloadArray, psEnd };
        //--------------------------------------------------------------------------------------------------------------

        class CJSONProtocol {
        public:
            static void ExceptionToJson(int ErrorCode, const std::exception &e, CJSON& Json);

            static bool Request(const CString &String, CJSONMessage &Message);
            static void Response(const CJSONMessage &Message, CString &String);

            static void PrepareResponse(const CJSONMessage &Request, CJSONMessage &Response);

            static CJSONMessage Call(const CString &UniqueId, const CString &Action, const CJSON &Payload);
            static CJSONMessage CallResult(const CString &UniqueId, const CJSON &Payload);
            static CJSONMessage CallError(const CString &UniqueId, const CString &ErrorCode, const CString &ErrorDescription, const CJSON &Payload);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        enum CResetType { rtHard = 0, rtSoft };

        enum CMessageTrigger { mtBootNotification = 0, mtDiagnosticsStatusNotification, mtFirmwareStatusNotification,
            mtHeartbeat, mtMeterValues, mtStatusNotification };

        enum CAuthorizationStatus { asAccepted = 0, asBlocked, asExpired, asInvalid, asConcurrentTx };

        enum CRegistrationStatus { rsAccepted = 0, rsPending, rsRejected };

        enum CTriggerMessageStatus { tmsAccepted = 0, tmsRejected, tmsNotImplemented };

        enum CConfigurationStatus { csAccepted = 0, csRejected, csRebootRequired, csNotSupported };

        enum CRemoteStartStopStatus { rssAccepted = 0, rssRejected };

        enum CReservationStatus { rvsAccepted = 0, rvsFaulted, rvsOccupied, rvsRejected, rvsUnavailable };

        enum CCancelReservationStatus { crsAccepted = 0, crsRejected };

        enum CClearCacheStatus { ccsAccepted = 0, ccsRejected };

        enum CChargePointStatus { cpsAvailable = 0, cpsPreparing, cpsCharging, cpsSuspendedEVSE, cpsSuspendedEV, cpsFinishing, cpsReserved, cpsUnavailable, cpsFaulted };

        enum CChargePointErrorCode { cpeConnectorLockFailure = 0, cpeHighTemperature, cpeMode3Error, cpeNoError,
            cpePowerMeterFailure, cpePowerSwitchFailure, cpeReaderFailure, cpeResetFailure, cpeGroundFailure,
            cpeOverCurrentFailure, cpeUnderVoltage, cpeWeakSignal, cpeOtherError };

        //--------------------------------------------------------------------------------------------------------------

        class CCSChargingPoint;
        //--------------------------------------------------------------------------------------------------------------

        class COCPPMessage {
        public:

            static CMessageType StringToMessageTypeId(const CString& Value);
            static CString MessageTypeIdToString(CMessageType Value);

            static CResetType StringToResetType(const CString& Value);
            static CString ResetTypeToString(CResetType Value);

            static CAuthorizationStatus StringToAuthorizationStatus(const CString& Value);
            static CString AuthorizationStatusToString(CAuthorizationStatus Value);

            static CMessageTrigger StringToMessageTrigger(const CString& String);
            static CString MessageTriggerToString(CMessageTrigger Value);

            static CRegistrationStatus StringToRegistrationStatus(const CString& Value);
            static CString RegistrationStatusToString(CRegistrationStatus Value);

            static CRemoteStartStopStatus StringToRemoteStartStopStatus(const CString& Value);
            static CString RemoteStartStopStatusToString(CRemoteStartStopStatus Value);

            static CReservationStatus StringToReservationStatus(const CString& Value);
            static CString ReservationStatusToString(CReservationStatus Value);

            static CCancelReservationStatus StringToCancelReservationStatus(const CString& Value);
            static CString CancelReservationStatusToString(CCancelReservationStatus Value);

            static CClearCacheStatus StringToClearCacheStatus(const CString& Value);
            static CString ClearCacheStatusToString(CClearCacheStatus Value);

            static CTriggerMessageStatus StringToTriggerMessageStatus(const CString& Value);
            static CString TriggerMessageStatusToString(CTriggerMessageStatus Value);

            static CConfigurationStatus StringToConfigurationStatus(const CString& Value);
            static CString ConfigurationStatusToString(CConfigurationStatus Value);

            static CChargePointStatus StringToChargePointStatus(const CString& Value);
            static CString ChargePointStatusToString(CChargePointStatus Value);

            static CChargePointErrorCode StringToChargePointErrorCode(const CString& Value);
            static CString ChargePointErrorCodeToString(CChargePointErrorCode Value);

            static bool Parse(CCSChargingPoint *APoint, const CSOAPMessage &Request, CSOAPMessage &Response);
            static bool Parse(CCSChargingPoint *APoint, const CJSONMessage &Request, CJSONMessage &Response);

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
            CChargePointStatus status = cpsUnavailable;
            CChargePointErrorCode errorCode = cpeNoError;
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

        typedef struct MeterValuesRequest {

            CString connectorId;
            CString transactionId;
            CJSON meterValue;

            MeterValuesRequest& operator<< (const CStringPairs& Value) {

                connectorId = Value["connectorId"];
                transactionId = Value["transactionId"];
                meterValue << Value["meterValue"];

                return *this;
            }

            MeterValuesRequest& operator<< (const CJSON& Value) {

                connectorId = Value["connectorId"].AsString();
                transactionId = Value["transactionId"].AsString();
                meterValue << Value["meterValue"];

                return *this;
            }

        } CMeterValuesRequest;

        //--------------------------------------------------------------------------------------------------------------

        //-- CActionHandler --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCustomChargingPoint;
        //--------------------------------------------------------------------------------------------------------------

        typedef std::function<void (CCustomChargingPoint *Sender, const CSOAPMessage &Request, CSOAPMessage &Response)> COnSOAPMessageHandlerEvent;
        typedef std::function<void (CCustomChargingPoint *Sender, const CJSONMessage &Request, CJSONMessage &Response)> COnJSONMessageHandlerEvent;
        //--------------------------------------------------------------------------------------------------------------

        class CSOAPActionHandler: CObject {
        private:

            bool m_Allow;

            COnSOAPMessageHandlerEvent m_Handler;

        public:

            CSOAPActionHandler(bool Allow, COnSOAPMessageHandlerEvent && Handler): CObject(),
                m_Allow(Allow), m_Handler(Handler) {

            };

            bool Allow() const { return m_Allow; };

            void Handler(CCustomChargingPoint *Sender, const CSOAPMessage &Request, CSOAPMessage &Response) {
                if (m_Allow && m_Handler)
                    m_Handler(Sender, Request, Response);
            }

        };
        //--------------------------------------------------------------------------------------------------------------

        class CJSONActionHandler: CObject {
        private:

            bool m_Allow;

            COnJSONMessageHandlerEvent m_Handler;

        public:

            CJSONActionHandler(bool Allow, COnJSONMessageHandlerEvent && Handler): CObject(),
                m_Allow(Allow), m_Handler(Handler) {

            };

            bool Allow() const { return m_Allow; };

            void Handler(CCustomChargingPoint *Sender, const CJSONMessage &Request, CJSONMessage &Response) {
                if (m_Allow && m_Handler)
                    m_Handler(Sender, Request, Response);
            }

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessageHandler ---------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class COCPPMessageHandlerManager;
        class COCPPMessageHandler;
        //--------------------------------------------------------------------------------------------------------------

        typedef std::function<void (COCPPMessageHandler *Handler, CWebSocketConnection *Connection)> COnMessageHandlerEvent;
        //--------------------------------------------------------------------------------------------------------------

        class COCPPMessageHandler: public CCollectionItem {
        private:

            CJSONMessage m_Message;

            COnMessageHandlerEvent m_Handler;

        public:

            COCPPMessageHandler(COCPPMessageHandlerManager *AManager, const CJSONMessage &Message, COnMessageHandlerEvent && Handler);

            const CJSONMessage &Message() const { return m_Message; }

            void Handler(CWebSocketConnection *AConnection);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessageHandlerManager --------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class COCPPMessageHandlerManager: public CCollection {
            typedef CCollection inherited;

        private:

            CCustomChargingPoint *m_pChargingPoint;

            COCPPMessageHandler *Get(int Index) const;
            void Set(int Index, COCPPMessageHandler *Value);

        public:

            explicit COCPPMessageHandlerManager(CCustomChargingPoint *AConnection): CCollection(this), m_pChargingPoint(AConnection) {

            }

            COCPPMessageHandler *Send(const CJSONMessage &Message, COnMessageHandlerEvent &&Handler, uint32_t Key = 0);

            COCPPMessageHandler *First() { return Get(0); };
            COCPPMessageHandler *Last() { return Get(Count() - 1); };

            COCPPMessageHandler *FindMessageById(const CString &Value) const;

            COCPPMessageHandler *Handlers(int Index) const { return Get(Index); }
            void Handlers(int Index, COCPPMessageHandler *Value) { Set(Index, Value); }

            COCPPMessageHandler *operator[] (int Index) const override { return Handlers(Index); };

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint;
        class CCSChargingPoint;
        //--------------------------------------------------------------------------------------------------------------

        typedef std::function<void (CObject *Sender, const CSOAPMessage &Message)> COnChargingPointMessageSOAPEvent;
        typedef std::function<void (CObject *Sender, const CJSONMessage &Message)> COnChargingPointMessageJSONEvent;
        //--------------------------------------------------------------------------------------------------------------

        class CCustomChargingPoint: public CObject {
            friend CChargingPoint;
            friend CCSChargingPoint;

        private:

            CWebSocketConnection *m_pConnection;

            CProtocolType m_ProtocolType;

            CString m_Address;
            CString m_Identity;

            int m_UpdateCount;

            bool m_bUpdateConnected;

            COnChargingPointMessageSOAPEvent m_OnMessageSOAP;
            COnChargingPointMessageJSONEvent m_OnMessageJSON;

            void AddToConnection(CWebSocketConnection *AConnection);
            void DeleteFromConnection(CWebSocketConnection *AConnection);

            void SetProtocolType(CProtocolType Value);
            void SetUpdateConnected(bool Value);

        protected:

            COCPPMessageHandlerManager m_Messages {this};

            virtual void DoMessageSOAP(const CSOAPMessage &Message);
            virtual void DoMessageJSON(const CJSONMessage &Message);

        public:

            CCustomChargingPoint();
            explicit CCustomChargingPoint(CWebSocketConnection *AConnection);

            ~CCustomChargingPoint() override = default;

            bool Connected();

            void SwitchConnection(CWebSocketConnection *Value);

            void SendMessage(const CJSONMessage &Message, bool ASendNow = false);
            void SendMessage(const CJSONMessage &Message, COnMessageHandlerEvent &&Handler);

            void SendNotSupported(const CString &UniqueId, const CString &ErrorDescription, const CJSON &Payload = {});
            void SendProtocolError(const CString &UniqueId, const CString &ErrorDescription, const CJSON &Payload = {});
            void SendInternalError(const CString &UniqueId, const CString &ErrorDescription, const CJSON &Payload = {});

            void BeginUpdate() { m_UpdateCount++; }
            void EndUpdate() { m_UpdateCount--; }

            int UpdateCount() const { return m_UpdateCount; }

            CWebSocketConnection *Connection() { return m_pConnection; };

            COCPPMessageHandlerManager &Messages() { return m_Messages; };
            const COCPPMessageHandlerManager &Messages() const { return m_Messages; };

            CString &Address() { return m_Address; };
            const CString &Address() const { return m_Address; };

            CString &Identity() { return m_Identity; };
            const CString &Identity() const { return m_Identity; };

            void UpdateConnected(bool Value) { SetUpdateConnected(Value); };
            bool UpdateConnected() const { return m_bUpdateConnected; };

            void ProtocolType(CProtocolType Value) { SetProtocolType(Value); };
            CProtocolType ProtocolType() const { return m_ProtocolType; };

            static CCustomChargingPoint *FindOfConnection(CWebSocketConnection *AConnection);

            const COnChargingPointMessageSOAPEvent &OnMessageSOAP() const { return m_OnMessageSOAP; }
            void OnMessageSOAP(COnChargingPointMessageSOAPEvent && Value) { m_OnMessageSOAP = Value; }

            const COnChargingPointMessageJSONEvent &OnMessageJSON() const { return m_OnMessageJSON; }
            void OnMessageJSON(COnChargingPointMessageJSONEvent && Value) { m_OnMessageJSON = Value; }

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPointController ----------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint;
        //--------------------------------------------------------------------------------------------------------------

        class CChargingPointController: public CObject {
        private:

            CChargingPoint *m_pChargingPoint;

        public:

            explicit CChargingPointController(CChargingPoint *AChargingPoint): CObject(), m_pChargingPoint(AChargingPoint) {

            }

            ~CChargingPointController() override = default;

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPointConnector -----------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        struct CIdTagInfo {

            CDateTime expiryDate = 0;
            CString parentIdTag;
            CAuthorizationStatus status = asInvalid;

            CIdTagInfo() = default;

            CIdTagInfo(const CIdTagInfo &owner) {
                if (&owner != this) {
                    Assign(owner);
                }
            }

            explicit CIdTagInfo(const CJSONValue &Value) {
                *this << Value;
            }

            void Assign(const CIdTagInfo &owner) {
                expiryDate = owner.expiryDate;
                parentIdTag = owner.parentIdTag;
                status = owner.status;
            }

            void Clear() {
                expiryDate = 0;
                parentIdTag.Clear();
                status = asInvalid;
            }

            CIdTagInfo& operator= (const CIdTagInfo& idTagInfo) {
                if (&idTagInfo != this) {
                    Assign(idTagInfo);
                }
                return *this;
            }

            CIdTagInfo& operator<< (const CJSON& Value) {
                if (Value.HasOwnProperty("expiryDate"))
                    expiryDate = StrToDateTimeDef(Value["expiryDate"].AsString().c_str(), 0, "%04d-%02d-%02dT%02d:%02d:%02d");

                if (Value.HasOwnProperty("parentIdTag"))
                    parentIdTag = Value["parentIdTag"].AsBoolean();

                status = COCPPMessage::StringToAuthorizationStatus(Value["status"].AsString());

                return *this;
            }

        };
        //--------------------------------------------------------------------------------------------------------------

        enum CChargingVoltage { cvUnknown = -1, cvAC, cvDC };
        enum CChargingInterface { ciUnknown = -1, ciSchuko, ciType1, ciType2, ciCCS1, ciCCS2, ciCHAdeMO, ciCommando, ciTesla, ciSuperCharger };
        //--------------------------------------------------------------------------------------------------------------

        struct CCharging {

            CChargingVoltage voltage = cvUnknown;
            CChargingInterface interface = ciUnknown;

            CCharging() = default;

            CCharging(const CCharging &Charging) {
                if (&Charging != this) {
                    Assign(Charging);
                }
            }

            CCharging(CChargingVoltage Voltage, CChargingInterface Interface) {
                voltage = Voltage;
                interface = Interface;
            }

            void Assign(const CCharging &Charging) {
                voltage = Charging.voltage;
                interface = Charging.interface;
            }

            CCharging& operator= (const CCharging& Charging) {
                if (&Charging != this) {
                    Assign(Charging);
                }
                return *this;
            }

            void Clear() {
                voltage = cvUnknown;
                interface = ciUnknown;
            }

            static CChargingVoltage StringToChargingVoltage(const CString &String);
            static CChargingInterface StringToChargingInterface(const CString &String);

            static CString ChargingVoltageToString(CChargingVoltage voltage);
            static CString ChargingInterfaceToString(CChargingInterface interface);

        };
        //--------------------------------------------------------------------------------------------------------------

        typedef TPair<CIdTagInfo> CIdTag;
        //--------------------------------------------------------------------------------------------------------------

        class CChargingPointConnector: public CObject {
        private:

            CChargingPoint *m_pChargingPoint;

            int m_ConnectorId = 0;
            int m_TransactionId = -1;
            int m_ReservationId = -1;
            int m_MeterValue = 0;

            CDateTime m_ExpiryDate = 0;

            CIdTag m_IdTag {};
            CIdTag m_ReservationIdTag {};

            CCharging m_Charging;

            CChargePointStatus m_Status = cpsUnavailable;
            CChargePointErrorCode m_ErrorCode = cpeNoError;

            void SetStatus(CChargePointStatus Status);

        protected:

            void ContinueRemoteStartTransaction();

        public:

            CChargingPointConnector(): CObject(), m_pChargingPoint(nullptr) {

            };

            CChargingPointConnector(const CChargingPointConnector &owner): CChargingPointConnector() {
                if (&owner != this) {
                    Assign(owner);
                }
            }

            explicit CChargingPointConnector(CChargingPoint *chargingPoint, int connectorId, int meterValue): CChargingPointConnector() {
                m_pChargingPoint = chargingPoint;
                m_ConnectorId = connectorId;
                m_MeterValue = meterValue;
            }

            ~CChargingPointConnector() override = default;

            void Clear();

            void Assign(const CChargingPointConnector &connector) {
                m_ConnectorId = connector.m_ConnectorId;
                m_TransactionId = connector.m_TransactionId;
                m_ReservationId = connector.m_ReservationId;
                m_MeterValue = connector.m_MeterValue;
                m_Status = connector.m_Status;
                m_Charging = connector.m_Charging;
                m_pChargingPoint = connector.m_pChargingPoint;
            }

            CChargingPointConnector& operator= (const CChargingPointConnector& connector) {
                if (&connector != this) {
                    Assign(connector);
                }
                return *this;
            }

            int ConnectorId() const { return m_ConnectorId; };
            CDateTime ExpiryDate() const { return m_ExpiryDate; };

            int TransactionId() const { return m_TransactionId; };
            void TransactionId(int Value) { m_TransactionId = value; };

            int ReservationId() const { return m_ReservationId; };
            void ReservationId(int Value) { m_ReservationId = value; };

            int MeterValue() const { return m_MeterValue; };

            void IncMeterValue(int delta);
            void UpdateStatusNotification();

            CRemoteStartStopStatus RemoteStartTransaction(const CString &idTag);
            CRemoteStartStopStatus RemoteStopTransaction();

            CReservationStatus ReserveNow(CDateTime expiryDate, const CString &idTag, const CString &parentIdTag, int reservationId);
            CCancelReservationStatus CancelReservation(int reservationId);

            bool MeterValues();

            CChargePointStatus Status() const { return m_Status; };
            void Status(CChargePointStatus Value) { SetStatus(Value); };

            CChargePointErrorCode &ErrorCode() { return m_ErrorCode; };
            const CChargePointErrorCode &ErrorCode() const { return m_ErrorCode; };

            CCharging &Charging() { return m_Charging; };
            const CCharging &Charging() const { return m_Charging; };

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        typedef TList<CChargingPointConnector> CChargingPointConnectors;
        //--------------------------------------------------------------------------------------------------------------

        struct CConfigurationKey {

            CString key;
            CString value;

            bool readonly = false;

            CConfigurationKey() = default;

            CConfigurationKey(const CConfigurationKey &owner) {
                if (&owner != this) {
                    Assign(owner);
                }
            }

            CConfigurationKey(const CString &key, const CString &value, bool readonly = true) {
                this->key = key;
                this->readonly = readonly;
                this->value = value;
            }

            explicit CConfigurationKey(const CJSONValue &Value) {
                *this << Value;
            }

            void Assign(const CConfigurationKey &owner) {
                key = owner.key;
                readonly = owner.readonly;
                value = owner.value;
            }

            CConfigurationKey& operator= (const CConfigurationKey& configurationKey) {
                if (&configurationKey != this) {
                    Assign(configurationKey);
                }
                return *this;
            }

            CConfigurationKey& operator<< (const CJSONValue& Value) {
                key = Value["key"].AsString();
                readonly = Value["readonly"].AsBoolean();
                value = Value["value"].AsString();

                return *this;
            }

        };
        //--------------------------------------------------------------------------------------------------------------

        typedef TPairs<CConfigurationKey> CConfigurationKeys;
        typedef TPairs<CIdTagInfo> CAuthorizationCache;
        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint: public CCustomChargingPoint {
        private:

            CString m_Prefix;

            CChargingPointController m_Controller {this};

            CChargingPointConnector m_ConnectorZero { this, 0, 0};

            CChargingPointConnectors m_Connectors;

            CConfigurationKeys m_ConfigurationKeys;

            CRegistrationStatus m_BootNotificationStatus;

            CAuthorizationCache m_AuthorizationCache;

        protected:

            void Load(const CString &FileName);
            void Save(const CString &FileName);

            void SetBootNotificationStatus(CRegistrationStatus Status);

        public:

            CChargingPoint();

            ~CChargingPoint() override;

            static CJSONMessage RequestToMessage(CWebSocketConnection *AWSConnection);

            int IndexOfConnectorId(int connectorId) const;
            int IndexOfTransactionId(int transactionId) const;
            int IndexOfReservationId(int reservationId) const;

            void SetConnectors(const CJSON &connectors);
            void SetConfigurationKeys(const CJSON &keys);

            void Save();
            void Reload();

            void Ping();

            CString &Prefix() { return m_Prefix; };
            const CString &Prefix() const { return m_Prefix; };

            CRegistrationStatus BootNotificationStatus() const { return m_BootNotificationStatus; }

            CChargingPointController &Controller() { return m_Controller; };
            const CChargingPointController &Controller() const { return m_Controller; };

            CChargingPointConnector &ConnectorZero() { return m_ConnectorZero; };
            const CChargingPointConnector &ConnectorZero() const { return m_ConnectorZero; };

            CChargingPointConnectors &Connectors() { return m_Connectors; };
            const CChargingPointConnectors &Connectors() const { return m_Connectors; };

            CConfigurationKeys &ConfigurationKeys() { return m_ConfigurationKeys; }
            const CConfigurationKeys &ConfigurationKeys() const { return m_ConfigurationKeys; }

            CAuthorizationCache &AuthorizationCache() { return m_AuthorizationCache; }
            const CAuthorizationCache &AuthorizationCache() const { return m_AuthorizationCache; }

            /// CSS -> CP

            /// 4.1. Authorize
            CIdTag AuthorizeLocal(const CString &idTag);
            void UpdateAuthorizationCache(const CIdTag &IdTag);

            /// 4.7. Meter Values
            bool MeterValues(int connectorId = -1);

            /// 4.9. Status Notification
            void StatusNotification(int connectorId);

            /// 5.1. Cancel Reservation
            CCancelReservationStatus CancelReservation(int reservationId);

            /// 5.3. Change Configuration
            CConfigurationStatus ChangeConfiguration(const CString &key, const CString &value);

            /// 5.4. Clear Cache
            CClearCacheStatus ClearCache();

            /// 5.8. Get Configuration
            CJSON ConnectorToJson(int connectorId = -1);
            CJSON ConfigurationToJson(const CString &key = {});

            /// 5.11. Remote Start Transaction
            CRemoteStartStopStatus RemoteStartTransaction(const CString &idTag, int connectorId = 0, const CJSON &chargingProfile = {});

            /// 5.12. Remote Stop Transaction
            CRemoteStartStopStatus RemoteStopTransaction(int transactionId = 0);

            /// 5.13. Reserve Now
            CReservationStatus ReserveNow(int connectorId, CDateTime expiryDate, const CString &idTag, const CString &parentIdTag, int reservationId);

            /// 5.14. Reset
            void Reset();

            /// 5.17. Trigger Message
            CTriggerMessageStatus TriggerMessage(CMessageTrigger requestedMessage, int connectorId = 0);

            /// CP -> CSS

            /// 4.1. Authorize
            void SendAuthorize(const CString &idTag, COnMessageHandlerEvent &&OnRequest);

            /// 4.2. Boot Notification
            void SendBootNotification();

            /// 4.6. Heartbeat
            void SendHeartbeat();

            /// 4.7. Meter Values
            void SendMeterValues(int connectorId, int transactionId, const CJSONArray &meterValue);

            /// 4.8. Start Transaction
            void SendStartTransaction(int connectorId, const CString &idTag, int meterStart, int reservationId, COnMessageHandlerEvent &&OnRequest);

            /// 4.9. Status Notification
            void SendStatusNotification(int connectorId, CChargePointStatus status, CChargePointErrorCode errorCode);

            /// 4.10. Stop Transaction
            void SendStopTransaction(const CString &idTag, int meterStop, int transactionId, const CString &reason, COnMessageHandlerEvent &&OnRequest);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSChargingPoint ------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCSChargingPointManager;
        //--------------------------------------------------------------------------------------------------------------

        class CCSChargingPoint: public CCollectionItem, public CCustomChargingPoint {
        private:

            CAuthorizeRequest m_AuthorizeRequest;
            CStartTransactionRequest m_StartTransactionRequest;
            CStopTransactionRequest m_StopTransactionRequest;
            CBootNotificationRequest m_BootNotificationRequest;
            CStatusNotificationRequest m_StatusNotificationRequest;
            CDataTransferRequest m_DataTransferRequest;
            CMeterValuesRequest m_MeterValuesRequest;

            bool ParseSOAP(const CString &Request, CString &Response);
            bool ParseJSON(const CString &Request, CString &Response);

        public:

            explicit CCSChargingPoint(CCSChargingPointManager *AManager, CWebSocketConnection *AConnection);

            ~CCSChargingPoint() override = default;

            const CAuthorizeRequest &AuthorizeRequest() const { return m_AuthorizeRequest; }
            const CStartTransactionRequest &StartTransactionRequest() const { return m_StartTransactionRequest; }
            const CStopTransactionRequest &StopTransactionRequest() const { return m_StopTransactionRequest; }
            const CBootNotificationRequest &BootNotificationRequest() const { return m_BootNotificationRequest; }
            const CStatusNotificationRequest &StatusNotificationRequest() const { return m_StatusNotificationRequest; }
            const CDataTransferRequest &DataTransferRequest() const { return m_DataTransferRequest; }
            const CMeterValuesRequest &MeterValuesRequest() const { return m_MeterValuesRequest; }

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

            void MeterValues(const CSOAPMessage &Request, CSOAPMessage &Response);
            void MeterValues(const CJSONMessage &Request, CJSONMessage &Response);

            bool Parse(const CString &Request, CString &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CCSChargingPointManager -----------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCSChargingPointManager: public CCollection {
            typedef CCollection inherited;

        private:

            CCSChargingPoint *Get(int Index) const;
            void Set(int Index, CCSChargingPoint *Value);

        public:

            CCSChargingPointManager(): CCollection(this) {

            }

            CCSChargingPoint *Add(CWebSocketConnection *AConnection);

            CCSChargingPoint *First() { return Get(0); };
            CCSChargingPoint *Last() { return Get(Count() - 1); };

            CCSChargingPoint *FindPointByIdentity(const CString &Value);
            CCSChargingPoint *FindPointByConnection(CWebSocketConnection *Value);

            CCSChargingPoint *Points(int Index) const { return Get(Index); }
            void Points(int Index, CCSChargingPoint *Value) { Set(Index, Value); }

            CCSChargingPoint *operator[] (int Index) const override { return Points(Index); };

        };

    }
}

using namespace Apostol::ChargePoint;
}

#endif //OCPP_PROTOCOL_HPP
