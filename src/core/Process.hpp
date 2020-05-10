/*++

Library name:

  apostol-core

Module Name:

  Process.hpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_PROCESS_HPP
#define APOSTOL_PROCESS_HPP
//----------------------------------------------------------------------------------------------------------------------

#define PROCESS_NORESPAWN     -1
#define PROCESS_JUST_SPAWN    -2
#define PROCESS_RESPAWN       -3
#define PROCESS_JUST_RESPAWN  -4
#define PROCESS_DETACHED      -5
//----------------------------------------------------------------------------------------------------------------------

#define log_failure(msg) {                                  \
  if (GLog != nullptr)                                      \
    GLog->Error(APP_LOG_EMERG, 0, msg);                     \
  else                                                      \
    std::cerr << APP_NAME << ": " << (msg) << std::endl;    \
  exit(2);                                                  \
}                                                           \
//----------------------------------------------------------------------------------------------------------------------

void signal_handler(int signo, siginfo_t *siginfo, void *ucontext);
void signal_error(int signo, siginfo_t *siginfo, void *ucontext);
//----------------------------------------------------------------------------------------------------------------------

typedef enum ProcessType {
    ptMain, ptSingle, ptMaster, ptSignaller, ptNewBinary, ptWorker, ptHelper
} CProcessType;
//----------------------------------------------------------------------------------------------------------------------

static LPCTSTR PROCESS_TYPE_NAME[] = {
        _T("main"),
        _T("single"),
        _T("master"),
        _T("signaller"),
        _T("new binary"),
        _T("worker"),
        _T("helper")
};
//----------------------------------------------------------------------------------------------------------------------

typedef struct {
    char         *path;
    const char   *name;
    char *const  *argv;
    char *const  *envp;
} CExecuteContext;
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Process {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCustomProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCustomProcess: public CObject, public CGlobalComponent {
        private:

            pid_t m_Pid;

            CCustomProcess *m_pParent;

            CProcessType m_Type;

            int m_Status;
            CString m_ProcessName;

            bool m_respawn: true;
            bool m_just_spawn: true;
            bool m_detached: true;
            bool m_exiting: true;
            bool m_exited: true;

            pid_t m_NewBinary;

            bool m_fDaemonized;

            Pointer m_pData;

        protected:

            void SetPid(pid_t Value) { m_Pid = Value; };

            void SetProcessName(LPCTSTR Value) { m_ProcessName = Value; };
            void SetData(Pointer Value) { m_pData = Value; };
            void SetStatus(int Value) { m_Status = Value; };

            void SetRespawn(bool Value) { m_respawn = Value; };
            void SetJustSpawn(bool Value) { m_just_spawn = Value; };
            void SetDetached(bool Value) { m_detached = Value; };
            void SetExited(bool Value) { m_exited = Value; };
            void SetExiting(bool Value) { m_exiting = Value; };

        public:

            CCustomProcess(): CCustomProcess(ptMain, nullptr) {

            };

            explicit CCustomProcess(CProcessType AType): CCustomProcess(AType, nullptr) {

            };

            explicit CCustomProcess(CProcessType AType, CCustomProcess *AParent);

            ~CCustomProcess() override = default;

            virtual void BeforeRun() abstract;
            virtual void AfterRun() abstract;

            virtual void Run() abstract;

            virtual void Terminate();

            virtual void Assign(CCustomProcess *AProcess);

            static void ExecuteProcess(CExecuteContext *AContext);

            CProcessType Type() const { return m_Type; };

            pid_t Pid() const { return m_Pid; };
            void Pid(pid_t Value) { SetPid(Value); };

            pid_t ParentId();

            CCustomProcess *Parent() { return m_pParent; };

            CString& ProcessName() { return m_ProcessName; };
            const CString& ProcessName() const { return m_ProcessName; };
            void ProcessName(LPCTSTR Value) { SetProcessName(Value); };

            LPCTSTR GetProcessName() const { return m_ProcessName.c_str(); };

            Pointer Data() { return m_pData; };
            void Data(Pointer Value) { SetData(Value); };

            pid_t NewBinary() const { return m_NewBinary; };
            void NewBinary(pid_t Value) { m_NewBinary = Value; };

            bool Daemonized() const { return m_fDaemonized; };
            void Daemonized(bool Value) { m_fDaemonized = Value; };

            bool Respawn() const { return m_respawn; };
            void Respawn(bool Value) { SetRespawn(Value); };

            bool JustSpawn() const { return m_just_spawn; };
            void JustSpawn(bool Value) { SetJustSpawn(Value); };

            bool Detached() const { return m_detached; };
            void Detached(bool Value) { SetDetached(Value); };

            bool Exited() const { return m_exited; };
            void Exited(bool Value) { SetExited(Value); };

            bool Exiting() const { return m_exiting; };
            void Exiting(bool Value) { SetExiting(Value); };

            int Status() const { return m_Status; };
            void Status(int Value) { SetStatus(Value); };

        }; // class CCustomProcess

        //--------------------------------------------------------------------------------------------------------------

        //-- CSignalProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CSignalProcess: public CCustomProcess, public CSignals {
        private:

            CSignalProcess *m_pSignalProcess;

        protected:

            sig_atomic_t    sig_reap;
            sig_atomic_t    sig_sigio;
            sig_atomic_t    sig_sigalrm;
            sig_atomic_t    sig_terminate;
            sig_atomic_t    sig_quit;
            sig_atomic_t    sig_debug_quit;
            sig_atomic_t    sig_reconfigure;
            sig_atomic_t    sig_reopen;
            sig_atomic_t    sig_noaccept;
            sig_atomic_t    sig_change_binary;

            uint_t          sig_exiting;
            uint_t          sig_restart;
            uint_t          sig_noaccepting;

            void CreateSignals();

            virtual void ChildProcessGetStatus() abstract;

            void SetSignalProcess(CSignalProcess *Value);

        public:

            CSignalProcess(CProcessType AType, CCustomProcess *AParent);

            virtual CSignalProcess *SignalProcess() { return m_pSignalProcess; };

            void SignalHandler(int signo, siginfo_t *siginfo, void *ucontext) override;

            virtual void Quit();

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CServerProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CServerProcess: public CSignalProcess {
        private:

            CHTTPServer *m_pServer;
#ifdef WITH_POSTGRESQL
            CPQServer *m_pPQServer;
#endif
        protected:

            virtual void DoOptions(CCommand *ACommand);
            virtual void DoGet(CCommand *ACommand);
            virtual void DoHead(CCommand *ACommand);
            virtual void DoPost(CCommand *ACommand);
            virtual void DoPut(CCommand *ACommand);
            virtual void DoPatch(CCommand *ACommand);
            virtual void DoDelete(CCommand *ACommand);
            virtual void DoTrace(CCommand *ACommand);
            virtual void DoConnect(CCommand *ACommand);

            virtual void DoTimer(CPollEventHandler *AHandler) abstract;
            virtual bool DoExecute(CTCPConnection *AConnection) abstract;

            virtual void DoVerbose(CSocketEvent *Sender, CTCPConnection *AConnection, LPCTSTR AFormat, va_list args);
            virtual void DoAccessLog(CTCPConnection *AConnection);

            virtual void DoServerListenException(CSocketEvent *Sender, Delphi::Exception::Exception *AException);
            virtual void DoServerException(CTCPConnection *AConnection, Delphi::Exception::Exception *AException);
            virtual void DoServerEventHandlerException(CPollEventHandler *AHandler, Delphi::Exception::Exception *AException);

            virtual void DoServerConnected(CObject *Sender);
            virtual void DoServerDisconnected(CObject *Sender);

            virtual void DoClientConnected(CObject *Sender);
            virtual void DoClientDisconnected(CObject *Sender);

            virtual void DoNoCommandHandler(CSocketEvent *Sender, LPCTSTR AData, CTCPConnection *AConnection);

            void SetServer(CHTTPServer *Value);

#ifdef WITH_POSTGRESQL
            virtual void DoPQServerException(CPQServer *AServer, Delphi::Exception::Exception *AException);
            virtual void DoPQConnectException(CPQConnection *AConnection, Delphi::Exception::Exception *AException);

            virtual void DoPQError(CPQConnection *AConnection);
            virtual void DoPQStatus(CPQConnection *AConnection);
            virtual void DoPQPollingStatus(CPQConnection *AConnection);

            virtual void DoPQReceiver(CPQConnection *AConnection, const PGresult *AResult);
            virtual void DoPQProcessor(CPQConnection *AConnection, LPCSTR AMessage);

            virtual void DoPQConnect(CObject *Sender);
            virtual void DoPQDisconnect(CObject *Sender);

            virtual void DoPQSendQuery(CPQQuery *AQuery);
            virtual void DoPQResultStatus(CPQResult *AResult);
            virtual void DoPQResult(CPQResult *AResult, ExecStatusType AExecStatus);

            void SetPQServer(CPQServer *Value);
#endif
        public:

            CServerProcess(CProcessType AType, CCustomProcess *AParent);

            void InitializeHandlers(CCommandHandlers *AHandlers, bool ADisconnect = false);

            CHTTPServer *Server() { return m_pServer; };
            void Server(CHTTPServer *Value) { SetServer(Value); };
#ifdef WITH_POSTGRESQL
            CPQServer *PQServer() { return m_pPQServer; };
            void PQServer(CPQServer *Value) { SetPQServer(Value); };

            virtual CPQPollQuery *GetQuery(CPollConnection *AConnection);

            bool ExecSQL(const CStringList &SQL, CPollConnection *AConnection = nullptr,
                         COnPQPollQueryExecutedEvent && OnExecuted = nullptr,
                         COnPQPollQueryExceptionEvent && OnException = nullptr);
#endif
            CHTTPClient * GetClient(const CString &Host, uint16_t Port);

            static void DebugRequest(CRequest *ARequest);
            static void DebugReply(CReply *AReply);

        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CModuleProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CModuleProcess: public CModuleManager, public CServerProcess {
        protected:

            void DoBeforeExecuteModule(CApostolModule *AModule) override;
            void DoAfterExecuteModule(CApostolModule *AModule) override;

            void DoTimer(CPollEventHandler *AHandler) override;
            bool DoExecute(CTCPConnection *AConnection) override;

        public:

            CModuleProcess(CProcessType AType, CCustomProcess *AParent);

            ~CModuleProcess() override = default;
        };

    }
//----------------------------------------------------------------------------------------------------------------------
}

using namespace Apostol::Process;
}
//----------------------------------------------------------------------------------------------------------------------

#endif //APOSTOL_PROCESS_HPP
