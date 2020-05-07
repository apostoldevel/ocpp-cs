/*++

Library name:

  apostol-core

Module Name:

  Module.hpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_MODULE_HPP
#define APOSTOL_MODULE_HPP

#define APOSTOL_MODULE_UID_LENGTH    42

extern "C++" {

namespace Apostol {

    namespace Module {

        typedef TList<CStringPairs> CQueryResult;
        typedef TList<CQueryResult> CQueryResults;
        //--------------------------------------------------------------------------------------------------------------

        typedef std::function<void (CHTTPServerConnection *AConnection)> COnMethodHandlerEvent;
        //--------------------------------------------------------------------------------------------------------------

        CString GetUID(unsigned int len);
        CString ApostolUID();

        //--------------------------------------------------------------------------------------------------------------

        //-- CAuthorization --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        enum CAuthorizationSchemes { asUnknown, asBasic, asSession };
        //--------------------------------------------------------------------------------------------------------------

        typedef struct CAuthorization {

            CAuthorizationSchemes Schema;

            CString Username;
            CString Password;
            CString Session;

            CAuthorization(): Schema(asUnknown) {

            }

            explicit CAuthorization(const CString& String): CAuthorization() {
                Parse(String);
            }

            void Parse(const CString& String) {
                if (String.IsEmpty())
                    throw Delphi::Exception::Exception("Authorization error: Data not found.");

                if (String.SubString(0, 5) == "Basic") {
                    const CString LPassphrase(base64_decode(String.SubString(6)));

                    const size_t LPos = LPassphrase.Find(':');
                    if (LPos == CString::npos)
                        throw Delphi::Exception::Exception("Authorization error: Incorrect passphrase.");

                    Schema = asBasic;
                    Username = LPassphrase.SubString(0, LPos);
                    Password = LPassphrase.SubString(LPos + 1);

                    if (Username.IsEmpty() || Password.IsEmpty())
                        throw Delphi::Exception::Exception("Authorization error: Username and password has not be empty.");
                } else if (String.SubString(0, 7) == "Session") {
                    Schema = asSession;
                    Session = String.SubString(8);
                    if (Session.Length() != 40)
                        throw Delphi::Exception::Exception("Authorization error: Incorrect Session length.");
                } else {
                    throw Delphi::Exception::Exception("Authorization error: Unknown schema.");
                }
            }

            CAuthorization &operator << (const CString& String) {
                Parse(String);
                return *this;
            }

        } CAuthorization;

        //--------------------------------------------------------------------------------------------------------------

        //-- CMethodHandler --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CMethodHandler: CObject {
        private:

            bool m_Allow;
            COnMethodHandlerEvent m_Handler;

        public:

            CMethodHandler(bool Allow, COnMethodHandlerEvent && Handler): CObject(),
                                                                          m_Allow(Allow), m_Handler(Handler) {

            };

            bool Allow() const { return m_Allow; };

            void Handler(CHTTPServerConnection *AConnection) {
                if (m_Allow && m_Handler)
                    m_Handler(AConnection);
            }
        };
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_POSTGRESQL
        class CJob: CCollectionItem {
        private:

            CString m_JobId;

            CString m_Result;

            CString m_CacheFile;

            CPQPollQuery *m_PollQuery;

        public:

            explicit CJob(CCollection *ACCollection);

            ~CJob() override = default;

            CString& JobId() { return m_JobId; };
            const CString& JobId() const { return m_JobId; };

            CString& CacheFile() { return m_CacheFile; };
            const CString& CacheFile() const { return m_CacheFile; };

            CString& Result() { return m_Result; }
            const CString& Result() const { return m_Result; }

            CPQPollQuery *PollQuery() { return m_PollQuery; };
            void PollQuery(CPQPollQuery *Value) { m_PollQuery = Value; };
        };
        //--------------------------------------------------------------------------------------------------------------

        class CJobManager: CCollection {
            typedef CCollection inherited;
        private:

            CJob *Get(int Index);
            void Set(int Index, CJob *Value);

        public:

            CJobManager(): CCollection(this) {

            }

            CJob *Add(CPQPollQuery *Query);

            CJob *FindJobById(const CString &Id);
            CJob *FindJobByQuery(CPQPollQuery *Query);

            CJob *Jobs(int Index) { return Get(Index); }
            void Jobs(int Index, CJob *Value) { Set(Index, Value); }

        };
#endif
        //--------------------------------------------------------------------------------------------------------------

        //-- CApostolModule --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CModuleManager;
        //--------------------------------------------------------------------------------------------------------------

        class CApostolModule: public CCollectionItem, public CGlobalComponent {
        private:

            CString m_AllowedMethods;
            CString m_AllowedHeaders;

            const CString& GetAllowedMethods(CString& AllowedMethods) const;
            const CString& GetAllowedHeaders(CString& AllowedHeaders) const;

        protected:

            CStringList *m_pMethods;
            CStringList m_Headers;

            virtual void CORS(CHTTPServerConnection *AConnection);

            virtual void DoOptions(CHTTPServerConnection *AConnection);

            virtual void MethodNotAllowed(CHTTPServerConnection *AConnection);
#ifdef WITH_POSTGRESQL
            virtual void DoPostgresQueryExecuted(CPQPollQuery *APollQuery) abstract;
            virtual void DoPostgresQueryException(CPQPollQuery *APollQuery, Delphi::Exception::Exception *AException) abstract;
#endif
        public:

            CApostolModule();

            explicit CApostolModule(CModuleManager *AManager);

            ~CApostolModule() override;

            virtual void InitMethods() abstract;

            virtual bool CheckUserAgent(const CString& Value) abstract;

            virtual void BeforeExecute(Pointer Data) abstract;
            virtual void AfterExecute(Pointer Data) abstract;

            virtual void Heartbeat() abstract;
            virtual void Execute(CHTTPServerConnection *AConnection) abstract;

            const CString& AllowedMethods() { return GetAllowedMethods(m_AllowedMethods); };
            const CString& AllowedHeaders() { return GetAllowedHeaders(m_AllowedHeaders); };
#ifdef WITH_POSTGRESQL
            static void EnumQuery(CPQResult *APQResult, CQueryResult& AResult);
            static void QueryToResults(CPQPollQuery *APollQuery, CQueryResults& AResults);

            CPQPollQuery *GetQuery(CPollConnection *AConnection);

            bool ExecSQL(const CStringList &SQL, CPollConnection *AConnection = nullptr,
                         COnPQPollQueryExecutedEvent && OnExecuted = nullptr,
                         COnPQPollQueryExceptionEvent && OnException = nullptr);
#endif
            static CHTTPClient * GetClient(const CString &Host, uint16_t Port);

            static void DebugRequest(CRequest *ARequest);
            static void DebugReply(CReply *AReply);
            static void DebugConnection(CHTTPServerConnection *AConnection);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CModuleManager --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CModuleManager: public CCollection {
            typedef CCollection inherited;

        protected:

            virtual void DoBeforeExecuteModule(CApostolModule *AModule) abstract;
            virtual void DoAfterExecuteModule(CApostolModule *AModule) abstract;

        public:

            explicit CModuleManager(): CCollection(this) {

            };

            void HeartbeatModules();

            bool ExecuteModules(CTCPConnection *AConnection);

            int ModuleCount() { return inherited::Count(); };
            void DeleteModule(int Index) { inherited::Delete(Index); };

            CApostolModule *Modules(int Index) { return (CApostolModule *) inherited::GetItem(Index); }
            void Modules(int Index, CApostolModule *Value) { inherited::SetItem(Index, (CCollectionItem *) Value); }
        };

    }
}

using namespace Apostol::Module;
}
//----------------------------------------------------------------------------------------------------------------------

#endif //APOSTOL_MODULE_HPP
