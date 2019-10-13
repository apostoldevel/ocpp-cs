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

        typedef struct COCPPMessage {
            CStringPairs Headers;
            CString Notification;
            CStringPairs Values;
        } COCPPMessage;
        //--------------------------------------------------------------------------------------------------------------

        typedef struct CDataBase {
            CString Username;
            CString Password;
            CString Session;
        } CDataBase;

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CSOAPProtocol {
        public:

            static void Request(const CString &xmlString, COCPPMessage &Message);
            static void Response(const COCPPMessage &Message, CString &xmlString);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- COCPPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        typedef struct COCPPMessageItem {
            CString MessageID;
            CString From;
            CString To;
            CString Action;
        } COCPPMessageItem, *POCPPMessageItem;

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPMessage ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint;
        class CChargingPointManager;
        //--------------------------------------------------------------------------------------------------------------

        class CSOAPMessage {
        public:

            static void Prepare(CChargingPoint *APoint, const COCPPMessage &Request, COCPPMessage &Response);
            static void Parse(CChargingPoint *APoint, const COCPPMessage &Request, COCPPMessage &Response);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CChargingPoint --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CChargingPoint: public CCollectionItem {
        private:

            CString m_Address;
            CString m_Identity;
            CString m_iccid;
            CString m_Vendor;
            CString m_Model;
            CString m_SerialNumber;
            CString m_BoxSerialNumber;
            CString m_FirmwareVersion;

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

            CString &iccid() { return m_iccid; };
            const CString &iccid() const { return m_iccid; };

            CString &Vendor() { return m_Vendor; };
            const CString &Vendor() const { return m_Vendor; };

            CString &Model() { return m_Model; };
            const CString &Model() const { return m_Model; };

            CString &SerialNumber() { return m_SerialNumber; };
            const CString &SerialNumber() const { return m_SerialNumber; };

            CString &BoxSerialNumber() { return m_BoxSerialNumber; };
            const CString &BoxSerialNumber() const { return m_BoxSerialNumber; };

            CString &FirmwareVersion() { return m_FirmwareVersion; };
            const CString &FirmwareVersion() const { return m_FirmwareVersion; };

            void BootNotification(const COCPPMessage &Request, COCPPMessage &Response);
            void Heartbeat(const COCPPMessage &Request, COCPPMessage &Response);

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
