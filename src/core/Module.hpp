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

        typedef TList<TList<CStringPairs>> CQueryResult;
        //--------------------------------------------------------------------------------------------------------------

        typedef std::function<void (CHTTPServerConnection *AConnection)> COnMethodHandlerEvent;
        //--------------------------------------------------------------------------------------------------------------

        CString GetUID(unsigned int len);
        CString ApostolUID();
        //--------------------------------------------------------------------------------------------------------------

        class CMethodHandler: CObject {
        private:

            bool m_Allow;
            COnMethodHandlerEvent m_Handler;

        public:

            CMethodHandler(bool Allow, COnMethodHandlerEvent && Handler): CObject(),
                m_Allow(Allow), m_Handler(Handler) {

            };

            bool Allow() { return m_Allow; };

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

            CStringList m_Methods;
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

            ~CApostolModule() override = default;

            virtual void InitMethods() abstract;

            virtual bool CheckUserAgent(const CString& Value) abstract;

            virtual void BeforeExecute(Pointer Data) abstract;
            virtual void AfterExecute(Pointer Data) abstract;

            virtual void Execute(CHTTPServerConnection *AConnection) abstract;

            const CString& AllowedMethods() { return GetAllowedMethods(m_AllowedMethods); };
            const CString& AllowedHeaders() { return GetAllowedHeaders(m_AllowedHeaders); };

#ifdef WITH_POSTGRESQL

            static void QueryToResult(CPQPollQuery *APollQuery, CQueryResult& AResult);

            CPQPollQuery *GetQuery(CPollConnection *AConnection);

            bool ExecSQL(CPollConnection *AConnection, const CStringList &SQL,
                         COnPQPollQueryExecutedEvent && OnExecuted = nullptr,
                         COnPQPollQueryExceptionEvent && OnException = nullptr);
#endif
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

            bool ExecuteModule(CTCPConnection *AConnection);

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
