/*++

Library name:

  apostol-core

Module Name:

  Process.cpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Core.hpp"
#include "Process.hpp"
//----------------------------------------------------------------------------------------------------------------------

#define BT_BUF_SIZE 255
//----------------------------------------------------------------------------------------------------------------------

void signal_error(int signo, siginfo_t *siginfo, void *ucontext)
{
    void*       addr;
    void*       trace[BT_BUF_SIZE];
    int         i;
    int         size;
    char      **msg;

    GLog->Error(APP_LOG_CRIT, 0, "signal: %d (%s), addr: 0x%xL", signo, sys_siglist[signo], siginfo->si_addr);

#ifdef __x86_64__

#if __WORDSIZE == 64
    addr = (void*)((ucontext_t*)ucontext)->uc_mcontext.gregs[REG_RIP];
#else
    addr = (void*)((ucontext_t*)ucontext)->uc_mcontext.gregs[REG_EIP];
#endif

    size = backtrace(trace, BT_BUF_SIZE);

    GLog->Error(APP_LOG_CRIT, 0, "backtrace() returned %d addresses", size);

    //trace[0] = addr;

    msg = backtrace_symbols(trace, size);
    if (msg)
    {
        GLog->Error(APP_LOG_DEBUG, 0, "-= backtrace log =-");

        for (i = 0; i < size; ++i)
        {
            GLog->Error(APP_LOG_DEBUG, 0, "%s", msg[i]);
        }

        GLog->Error(APP_LOG_DEBUG, 0, "-= backtrace log =-");
        free(msg);
    }
#endif
    exit(3);
}
//----------------------------------------------------------------------------------------------------------------------

void signal_handler(int signo, siginfo_t *siginfo, void *ucontext)
{
    try
    {
        GApplication->SignalProcess()->SignalHandler(signo, siginfo, ucontext);
    }
    catch (std::exception& e)
    {
        log_failure(e.what());
    }
    catch (...)
    {
        log_failure("Unknown fatality error...");
    }
}
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Process {

        //--------------------------------------------------------------------------------------------------------------

        //-- CCustomProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCustomProcess::CCustomProcess(CProcessType AType, CCustomProcess *AParent): CObject(), CGlobalComponent(),
                m_Type(AType), m_pParent(AParent) {

            m_Pid = MainThreadID;

            m_fDaemonized = false;
            m_NewBinary = 0;

            m_Status = 0;

            m_respawn = false;
            m_just_spawn = false;
            m_detached = false;
            m_exiting = false;
            m_exited = false;

            m_pData = nullptr;

            m_ProcessName = PROCESS_TYPE_NAME[m_Type];
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomProcess::Assign(CCustomProcess *AProcess) {
            m_fDaemonized = AProcess->Daemonized();
            m_NewBinary = AProcess->NewBinary();
        }
        //--------------------------------------------------------------------------------------------------------------

        pid_t CCustomProcess::ParentId() {
            if (Assigned(m_pParent))
                return m_pParent->Pid();
            return -1;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomProcess::Terminate() {
            kill(m_Pid, signal_value(SIG_TERMINATE_SIGNAL));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomProcess::ExecuteProcess(CExecuteContext *AContext)
        {
            if (execve(AContext->path, AContext->argv, AContext->envp) == -1) {
                throw EOSError(errno, _T("execve() failed while executing %s \"%s\""), AContext->name, AContext->path);
            }

            exit(1);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CSignalProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CSignalProcess::CSignalProcess(CProcessType AType, CCustomProcess *AParent):
                CCustomProcess(AType, AParent), CSignals(), m_pSignalProcess(this) {

            sig_reap = 0;
            sig_sigio = 0;
            sig_sigalrm = 0;
            sig_terminate = 0;
            sig_quit = 0;
            sig_debug_quit = 0;
            sig_reconfigure = 0;
            sig_reopen = 0;
            sig_noaccept = 0;
            sig_change_binary = 0;

            sig_exiting = 0;
            sig_restart = 0;
            sig_noaccepting = 0;

            CreateSignals();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSignalProcess::SetSignalProcess(CSignalProcess *Value) {
            if (m_pSignalProcess != Value) {
                m_pSignalProcess = Value;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSignalProcess::CreateSignals() {
            if (Type() == ptSignaller) {

                AddSignal(signal_value(SIG_RECONFIGURE_SIGNAL), "SIG" sig_value(SIG_RECONFIGURE_SIGNAL), "reload", nullptr);
                AddSignal(signal_value(SIG_REOPEN_SIGNAL), "SIG" sig_value(SIG_REOPEN_SIGNAL), "reopen", nullptr);
                AddSignal(signal_value(SIG_TERMINATE_SIGNAL), "SIG" sig_value(SIG_TERMINATE_SIGNAL), "stop", nullptr);
                AddSignal(signal_value(SIG_SHUTDOWN_SIGNAL), "SIG" sig_value(SIG_SHUTDOWN_SIGNAL), "quit", nullptr);

            } else {

                AddSignal(signal_value(SIG_RECONFIGURE_SIGNAL), "SIG" sig_value(SIG_RECONFIGURE_SIGNAL),
                          "reload", signal_handler);

                AddSignal(signal_value(SIG_REOPEN_SIGNAL), "SIG" sig_value(SIG_REOPEN_SIGNAL),
                          "reopen", signal_handler);

                AddSignal(signal_value(SIG_NOACCEPT_SIGNAL), "SIG" sig_value(SIG_NOACCEPT_SIGNAL),
                          "", signal_handler);

                AddSignal(signal_value(SIG_TERMINATE_SIGNAL), "SIG" sig_value(SIG_TERMINATE_SIGNAL),
                          "stop", signal_handler);

                AddSignal(signal_value(SIG_SHUTDOWN_SIGNAL), "SIG" sig_value(SIG_SHUTDOWN_SIGNAL),
                          "quit", signal_handler);

                AddSignal(signal_value(SIG_CHANGEBIN_SIGNAL), "SIG" sig_value(SIG_CHANGEBIN_SIGNAL),
                          "", signal_handler);

                AddSignal(SIGINT, "SIGINT", nullptr, signal_handler);

                AddSignal(SIGIO, "SIGIO", nullptr, signal_handler);

                AddSignal(SIGCHLD, "SIGCHLD", nullptr, signal_handler);

                AddSignal(SIGSYS, "SIGSYS, SIG_IGN", nullptr, nullptr);

                AddSignal(SIGPIPE, "SIGPIPE, SIG_IGN", nullptr, nullptr);
            }

            if (Type() >= ptWorker) {

                AddSignal(SIGFPE, "SIGFPE", nullptr, signal_error);

                AddSignal(SIGILL, "SIGILL", nullptr, signal_error);

                AddSignal(SIGSEGV, "SIGSEGV", nullptr, signal_error);

                AddSignal(SIGBUS, "SIGBUS", nullptr, signal_error);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSignalProcess::SignalHandler(int signo, siginfo_t *siginfo, void *ucontext) {

            LPCTSTR      action;
            LPCTSTR      sigcode;
            int_t        ignore;
            int          err;

            ignore = 0;

            err = errno;

            int I = IndexOfSigNo(signo);
            if (I >= 0)
                sigcode = Signals(I)->Code();
            else
                sigcode = _T("signal code not found");

            action = _T("");

            log_debug3(APP_LOG_DEBUG_CORE, Log(), 0, "signal handler %d (%s), process name: %s", signo, sigcode, GetProcessName());

            switch (Type()) {
                case ptMaster:
                case ptSingle:
                    switch (signo) {

                        case signal_value(SIG_SHUTDOWN_SIGNAL):
                            sig_quit = 1;
                            action = _T(", shutting down");
                            break;

                        case signal_value(SIG_TERMINATE_SIGNAL):
                        case SIGINT:
                            sig_terminate = 1;
                            action = _T(", exiting");
                            break;

                        case signal_value(SIG_NOACCEPT_SIGNAL):
                            if (Daemonized()) {
                                sig_noaccept = 1;
                                action = _T(", stop accepting connections");
                            }
                            break;

                        case signal_value(SIG_RECONFIGURE_SIGNAL):
                            sig_reconfigure = 1;
                            action = _T(", reconfiguring");
                            break;

                        case signal_value(SIG_REOPEN_SIGNAL):
                            sig_reopen = 1;
                            action = _T(", reopening logs");
                            break;

                        case signal_value(SIG_CHANGEBIN_SIGNAL):
                            if (getppid() == ParentId() || NewBinary() > 0) {

                                /*
                                 * Ignore the signal in the new binary if its parent is
                                 * not changed, i.e. the old binary's process is still
                                 * running.  Or ignore the signal in the old binary's
                                 * process if the new binary's process is already running.
                                 */

                                action = _T(", ignoring");
                                ignore = 1;
                                break;
                            }

                            sig_change_binary = 1;
                            action = _T(", changing binary");
                            break;

                        case SIGALRM:
                            sig_sigalrm = 1;
                            action = _T(", alarm");
                            break;

                        case SIGIO:
                            sig_sigio = 1;
                            break;

                        case SIGCHLD:
                            sig_reap = 1;
                            break;

                        default:
                            break;
                    }

                    break;

                case ptWorker:
                case ptHelper:
                    switch (signo) {

                        case signal_value(SIG_NOACCEPT_SIGNAL):
                            if (!Daemonized()) {
                                break;
                            }
                            sig_debug_quit = 1;
                            /* fall through */
                        case signal_value(SIG_SHUTDOWN_SIGNAL):
                            sig_quit = 1;
                            action = _T(", shutting down");
                            break;

                        case signal_value(SIG_TERMINATE_SIGNAL):
                        case SIGINT:
                            sig_terminate = 1;
                            action = _T(", exiting");
                            break;

                        case signal_value(SIG_REOPEN_SIGNAL):
                            sig_reopen = 1;
                            action = _T(", reopening logs");
                            break;

                        case signal_value(SIG_RECONFIGURE_SIGNAL):
#ifdef _DEBUG
                            sig_terminate = 1;
                            action = _T(", exiting");
#else
                            action = _T(", ignoring");
#endif
                            break;
                        case signal_value(SIG_CHANGEBIN_SIGNAL):
                        case SIGIO:
                            action = _T(", ignoring");
                            break;

                        case SIGALRM:
                            sig_sigalrm = 1;
                            action = _T(", alarm");
                            break;

                        default:
                            break;
                    }

                    break;

                default:
                    break;
            }

            if (siginfo && siginfo->si_pid) {
                Log()->Error(APP_LOG_NOTICE, 0,
                             _T("signal %d (%s) received from %P%s"),
                             signo, sigcode, siginfo->si_pid, action);

            } else {
                Log()->Error(APP_LOG_NOTICE, 0,
                             _T("signal %d (%s) received%s"),
                             signo, sigcode, action);
            }

            if (ignore) {
                Log()->Error(APP_LOG_NOTICE, 0,
                             _T("the changing binary signal is ignored: ")
                             _T("you should shutdown or terminate ")
                             _T("before either old or new binary's process"));
            }

            if (signo == SIGCHLD) {
                log_debug0(APP_LOG_DEBUG_CORE, Log(), 0, "child process get status");
                ChildProcessGetStatus();
            }

            errno = err;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSignalProcess::Quit() {
            sig_quit = 1;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CServerProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CServerProcess::CServerProcess(CProcessType AType, CCustomProcess *AParent): CSignalProcess(AType, AParent) {
            m_pServer = nullptr;
#ifdef WITH_POSTGRESQL
            m_pPQServer = nullptr;
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::SetServer(CHTTPServer *Value) {
            if (m_pServer != Value) {

                if (Value == nullptr) {
                    delete m_pServer;
                }

                m_pServer = Value;
            }
        }
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_POSTGRESQL
        void CServerProcess::SetPQServer(CPQServer *Value) {
            if (m_pPQServer != Value) {

                if (Value == nullptr) {
                    delete m_pPQServer;
                }

                m_pPQServer = Value;
            }
        }
        //--------------------------------------------------------------------------------------------------------------
#endif
        void CServerProcess::InitializeHandlers(CCommandHandlers *AHandlers, bool ADisconnect) {

            if (Assigned(AHandlers)) {

                CCommandHandler *LCommand;

                AHandlers->ParseParamsDefault(false);
                AHandlers->DisconnectDefault(ADisconnect);

                LCommand =AHandlers->Add();
                LCommand->Command(_T("GET"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoGet, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("POST"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoPost, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("OPTIONS"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoOptions, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("PUT"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoPut, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("DELETE"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoDelete, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("HEAD"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoHead, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("PATCH"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoPatch, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("TRACE"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoTrace, this, _1));

                LCommand = AHandlers->Add();
                LCommand->Command(_T("CONNECT"));
                LCommand->OnCommand(std::bind(&CServerProcess::DoConnect, this, _1));
            }
        }
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_POSTGRESQL
        CPQPollQuery *CServerProcess::GetQuery(CPollConnection *AConnection) {
            CPQPollQuery *LQuery = nullptr;

            if (PQServer()->Active()) {
                LQuery = PQServer()->GetQuery();
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                LQuery->OnSendQuery([this](auto && AQuery) { DoPQSendQuery(AQuery); });
                LQuery->OnResultStatus([this](auto && AResult) { DoPQResultStatus(AResult); });
                LQuery->OnResult([this](auto && AResult, auto && AExecStatus) { DoPQResult(AResult, AExecStatus); });
#else
                LQuery->OnSendQuery(std::bind(&CServerProcess::DoPQSendQuery, this, _1));
                LQuery->OnResultStatus(std::bind(&CServerProcess::DoPQResultStatus, this, _1));
                LQuery->OnResult(std::bind(&CServerProcess::DoPQResult, this, _1, _2));
#endif
                LQuery->PollConnection(AConnection);
            }

            return LQuery;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CServerProcess::ExecSQL(const CStringList &SQL, CPollConnection *AConnection,
                                     COnPQPollQueryExecutedEvent &&OnExecuted,
                                     COnPQPollQueryExceptionEvent &&OnException) {

            auto LQuery = GetQuery(AConnection);

            if (LQuery == nullptr)
                throw Delphi::Exception::Exception("ExecSQL: GetQuery() failed!");

            if (OnExecuted != nullptr)
                LQuery->OnPollExecuted(static_cast<COnPQPollQueryExecutedEvent &&>(OnExecuted));

            if (OnException != nullptr)
                LQuery->OnException(static_cast<COnPQPollQueryExceptionEvent &&>(OnException));

            LQuery->SQL() = SQL;

            if (LQuery->Start() != POLL_QUERY_START_ERROR) {
                return true;
            } else {
                delete LQuery;
            }

            Log()->Error(APP_LOG_ALERT, 0, "ExecSQL: StartQuery() failed!");

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQReceiver(CPQConnection *AConnection, const PGresult *AResult) {
            const auto& Info = AConnection->ConnInfo();
            if (Info.ConnInfo().IsEmpty()) {
                Log()->Postgres(APP_LOG_INFO, _T("Receiver message: %s"), PQresultErrorMessage(AResult));
            } else {
                Log()->Postgres(APP_LOG_INFO, "[%d] [postgresql://%s@%s:%s/%s] Receiver message: %s", AConnection->Socket(),
                                Info["user"].c_str(), Info["host"].c_str(), Info["port"].c_str(), Info["dbname"].c_str(), PQresultErrorMessage(AResult));
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQProcessor(CPQConnection *AConnection, LPCSTR AMessage) {
            const auto& Info = AConnection->ConnInfo();
            if (Info.ConnInfo().IsEmpty()) {
                Log()->Postgres(APP_LOG_INFO, _T("Processor message: %s"), AMessage);
            } else {
                Log()->Postgres(APP_LOG_INFO, "[%d] [postgresql://%s@%s:%s/%s] Processor message: %s", AConnection->Socket(),
                                Info["user"].c_str(), Info["host"].c_str(), Info["port"].c_str(), Info["dbname"].c_str(), AMessage);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQConnectException(CPQConnection *AConnection, Delphi::Exception::Exception *AException) {
            Log()->Postgres(APP_LOG_EMERG, AException->what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQServerException(CPQServer *AServer, Delphi::Exception::Exception *AException) {
            Log()->Postgres(APP_LOG_EMERG, AException->what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQError(CPQConnection *AConnection) {
            Log()->Postgres(APP_LOG_EMERG, AConnection->GetErrorMessage());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQStatus(CPQConnection *AConnection) {
            Log()->Postgres(APP_LOG_DEBUG, AConnection->StatusString());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQPollingStatus(CPQConnection *AConnection) {
            Log()->Postgres(APP_LOG_DEBUG, AConnection->PollingStatusString());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQSendQuery(CPQQuery *AQuery) {
            for (int I = 0; I < AQuery->SQL().Count(); ++I) {
                Log()->Postgres(APP_LOG_INFO, AQuery->SQL()[I].c_str());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQResultStatus(CPQResult *AResult) {
            Log()->Postgres(APP_LOG_DEBUG, AResult->StatusString());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQResult(CPQResult *AResult, ExecStatusType AExecStatus) {
/*
            if (AExecStatus == PGRES_TUPLES_OK || AExecStatus == PGRES_SINGLE_TUPLE) {
                CString Print;

                Print = "(";
                Print += AResult->fName(0);
                for (int I = 1; I < AResult->nFields(); ++I) {
                    Print += ",";
                    Print += AResult->fName(I);
                }
                Print += ")";

                Log()->Postgres(APP_LOG_INFO, "%s", Print.c_str());

                Print = "(";
                for (int Row = 0; Row < AResult->nTuples(); ++Row) {

                    if (AResult->GetIsNull(Row, 0)) {
                        Print += "<null>";
                    } else {
                        if (AResult->fFormat(0) == 0) {
                            Print += AResult->GetValue(Row, 0);
                        } else {
                            Print += "<binary>";
                        }
                    }

                    for (int Col = 1; Col < AResult->nFields(); ++Col) {
                        Print += ",";
                        if (AResult->GetIsNull(Row, Col)) {
                            Print += "<null>";
                        } else {
                            if (AResult->fFormat(Col) == 0) {
                                Print += AResult->GetValue(Row, Col);
                            } else {
                                Print += "<binary>";
                            }
                        }
                    }
                }

                Print += ")";

                Log()->Postgres(APP_LOG_INFO, "%s", Print.c_str());
            } else {
                Log()->Postgres(APP_LOG_EMERG, AResult->GetErrorMessage());
            }
*/
            if (!(AExecStatus == PGRES_TUPLES_OK || AExecStatus == PGRES_SINGLE_TUPLE)) {
                Log()->Postgres(APP_LOG_EMERG, AResult->GetErrorMessage());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQConnect(CObject *Sender) {
            auto LConnection = dynamic_cast<CPQConnection *>(Sender);
            if (LConnection != nullptr) {
                const auto& Info = LConnection->ConnInfo();
                if (!Info.ConnInfo().IsEmpty()) {
                    Log()->Postgres(APP_LOG_NOTICE, "[%d] [postgresql://%s@%s:%s/%s] Connected.", LConnection->PID(),
                                    Info["user"].c_str(), Info["host"].c_str(), Info["port"].c_str(), Info["dbname"].c_str());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPQDisconnect(CObject *Sender) {
            auto LConnection = dynamic_cast<CPQConnection *>(Sender);
            if (LConnection != nullptr) {
                const auto& Info = LConnection->ConnInfo();
                if (!Info.ConnInfo().IsEmpty()) {
                    Log()->Postgres(APP_LOG_NOTICE, "[%d] [postgresql://%s@%s:%s/%s] Disconnected.", LConnection->PID(),
                                    Info["user"].c_str(), Info["host"].c_str(), Info["port"].c_str(), Info["dbname"].c_str());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------
#endif
        void CServerProcess::DebugRequest(CRequest *ARequest) {
            DebugMessage("[%p] Request:\n%s %s HTTP/%d.%d\n", ARequest, ARequest->Method.c_str(), ARequest->URI.c_str(), ARequest->VMajor, ARequest->VMinor);

            for (int i = 0; i < ARequest->Headers.Count(); i++)
                DebugMessage("%s: %s\n", ARequest->Headers[i].Name.c_str(), ARequest->Headers[i].Value.c_str());

            if (!ARequest->Content.IsEmpty())
                DebugMessage("\n%s\n", ARequest->Content.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DebugReply(CReply *AReply) {
            DebugMessage("[%p] Reply:\nHTTP/%d.%d %d %s\n", AReply, AReply->VMajor, AReply->VMinor, AReply->Status, AReply->StatusText.c_str());

            for (int i = 0; i < AReply->Headers.Count(); i++)
                DebugMessage("%s: %s\n", AReply->Headers[i].Name.c_str(), AReply->Headers[i].Value.c_str());

            if (!AReply->Content.IsEmpty())
                DebugMessage("\n%s\n", AReply->Content.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        CHTTPClient *CServerProcess::GetClient(const CString &Host, uint16_t Port) {
            auto LClient = new CHTTPClient(Host.c_str(), Port);

            LClient->ClientName() = GApplication->Title();

            LClient->PollStack(m_pServer->PollStack());
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            LClient->OnVerbose([this](auto && Sender, auto && AConnection, auto && AFormat, auto && args) { DoVerbose(Sender, AConnection, AFormat, args); });
            LClient->OnException([this](auto && AConnection, auto && AException) { DoServerException(AConnection, AException); });
            LClient->OnEventHandlerException([this](auto && AHandler, auto && AException) { DoServerEventHandlerException(AHandler, AException); });
            LClient->OnConnected([this](auto && Sender) { DoClientConnected(Sender); });
            LClient->OnDisconnected([this](auto && Sender) { DoClientDisconnected(Sender); });
            LClient->OnNoCommandHandler([this](auto && Sender, auto && AData, auto && AConnection) { DoNoCommandHandler(Sender, AData, AConnection); });
#else
            LClient->OnVerbose(std::bind(&CServerProcess::DoVerbose, this, _1, _2, _3, _4));
            LClient->OnException(std::bind(&CServerProcess::DoServerException, this, _1, _2));
            LClient->OnEventHandlerException(std::bind(&CServerProcess::DoServerEventHandlerException, this, _1, _2));
            LClient->OnConnected(std::bind(&CServerProcess::DoClientConnected, this, _1));
            LClient->OnDisconnected(std::bind(&CServerProcess::DoClientDisconnected, this, _1));
            LClient->OnNoCommandHandler(std::bind(&CServerProcess::DoNoCommandHandler, this, _1, _2, _3));
#endif
            return LClient;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoServerListenException(CSocketEvent *Sender, Delphi::Exception::Exception *AException) {
            Log()->Error(APP_LOG_EMERG, 0, AException->what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoServerException(CTCPConnection *AConnection,
                Delphi::Exception::Exception *AException) {
            Log()->Error(APP_LOG_EMERG, 0, AException->what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoServerEventHandlerException(CPollEventHandler *AHandler,
                Delphi::Exception::Exception *AException) {
            Log()->Error(APP_LOG_EMERG, 0, AException->what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoServerConnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (LConnection != nullptr) {
                Log()->Message(_T("[%s:%d] Client opened connection."), LConnection->Socket()->Binding()->PeerIP(),
                               LConnection->Socket()->Binding()->PeerPort());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoServerDisconnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *>(Sender);
            if (LConnection != nullptr) {
#ifdef WITH_POSTGRESQL
                auto LPollQuery = PQServer()->FindQueryByConnection(LConnection);
                if (LPollQuery != nullptr) {
                    LPollQuery->PollConnection(nullptr);
                }
#endif
                Log()->Message(_T("[%s:%d] Client closed connection."), LConnection->Socket()->Binding()->PeerIP(),
                               LConnection->Socket()->Binding()->PeerPort());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoClientConnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPClientConnection *>(Sender);
            if (LConnection != nullptr) {
                Log()->Message(_T("[%s:%d] Client connected."), LConnection->Socket()->Binding()->PeerIP(),
                               LConnection->Socket()->Binding()->PeerPort());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoClientDisconnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPClientConnection *>(Sender);
            if (LConnection != nullptr) {
                Log()->Message(_T("[%s:%d] Client disconnected."), LConnection->Socket()->Binding()->PeerIP(),
                               LConnection->Socket()->Binding()->PeerPort());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoVerbose(CSocketEvent *Sender, CTCPConnection *AConnection, LPCTSTR AFormat,
                va_list args) {
            Log()->Debug(0, AFormat, args);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoAccessLog(CTCPConnection *AConnection) {
            auto LConnection = dynamic_cast<CHTTPServerConnection *> (AConnection);
            auto LRequest = LConnection->Request();
            auto LReply = LConnection->Reply();

            TCHAR szTime[PATH_MAX / 4] = {0};

            time_t wtime = time(nullptr);
            struct tm *wtm = localtime(&wtime);

            if ((wtm != nullptr) && (strftime(szTime, sizeof(szTime), "%d/%b/%Y:%T %z", wtm) != 0)) {

                const CString &LReferer = LRequest->Headers.Values(_T("referer"));
                const CString &LUserAgent = LRequest->Headers.Values(_T("user-agent"));

                auto LBinding = LConnection->Socket()->Binding();
                if (LBinding != nullptr) {
                    Log()->Access(_T("%s %d %8.2f ms [%s] \"%s %s HTTP/%d.%d\" %d %d \"%s\" \"%s\"\r\n"),
                                  LBinding->PeerIP(), LBinding->PeerPort(),
                                  double((clock() - AConnection->Tag()) / (double) CLOCKS_PER_SEC * 1000), szTime,
                                  LRequest->Method.c_str(), LRequest->URI.c_str(), LRequest->VMajor, LRequest->VMinor,
                                  LReply->Status, LReply->Content.Size(),
                                  LReferer.IsEmpty() ? "-" : LReferer.c_str(),
                                  LUserAgent.IsEmpty() ? "-" : LUserAgent.c_str());
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoNoCommandHandler(CSocketEvent *Sender, LPCTSTR AData, CTCPConnection *AConnection) {
            Log()->Error(APP_LOG_EMERG, 0, "No command handler: %s", AData);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoGet(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPost(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoOptions(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoHead(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPut(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoPatch(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoDelete(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoTrace(CCommand *ACommand) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CServerProcess::DoConnect(CCommand *ACommand) {

        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CModuleProcess --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CModuleProcess::CModuleProcess(CProcessType AType, CCustomProcess *AParent): CModuleManager(),
            CServerProcess(AType, AParent) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CModuleProcess::DoBeforeExecuteModule(CApostolModule *AModule) {
            AModule->BeforeExecute(this);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CModuleProcess::DoAfterExecuteModule(CApostolModule *AModule) {
            AModule->AfterExecute(this);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CModuleProcess::DoTimer(CPollEventHandler *AHandler) {
            uint64_t exp;

            auto LTimer = dynamic_cast<CEPollTimer *> (AHandler->Binding());
            LTimer->Read(&exp, sizeof(uint64_t));

            try {
                HeartbeatModules();
            } catch (Delphi::Exception::Exception &E) {
                DoServerEventHandlerException(AHandler, &E);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CModuleProcess::DoExecute(CTCPConnection *AConnection) {
            bool Result = false;
            auto LConnection = dynamic_cast<CHTTPServerConnection *> (AConnection);

            try {
                clock_t start = clock();

                Result = ExecuteModules(LConnection);

                Log()->Debug(0, _T("[Module] Runtime: %.2f ms."), (double) ((clock() - start) / (double) CLOCKS_PER_SEC * 1000));
            } catch (Delphi::Exception::Exception &E) {
                DoServerException(LConnection, &E);
            }

            return Result;
        }
        //--------------------------------------------------------------------------------------------------------------

    }
}
}
