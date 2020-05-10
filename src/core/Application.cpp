/*++

Library name:

  apostol-core

Module Name:

  Application.cpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Core.hpp"
#include "Application.hpp"
//----------------------------------------------------------------------------------------------------------------------

#define APP_FILE_NOT_FOUND "File not found: %s"

extern "C++" {

namespace Apostol {

    namespace Application {

        CApplication *GApplication = nullptr;

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessManager -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CProcessManager::Start(CApplicationProcess *AProcess) {
            if (Assigned(AProcess)) {

                DoBeforeStartProcess(AProcess);
                try {
                    AProcess->BeforeRun();
                    AProcess->Run();
                    AProcess->AfterRun();
                } catch (std::exception& e) {
                    log_failure(e.what())
                } catch (...) {
                    log_failure("Unknown error...")
                }
                DoAfterStartProcess(AProcess);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessManager::Stop(int Index) {
            Process(Index)->Terminate();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessManager::StopAll() {
            for (int I = 0; I < Count(); I++)
                Stop(I);
        }
        //--------------------------------------------------------------------------------------------------------------

        CApplicationProcess *CProcessManager::FindProcessById(pid_t Pid) {
            CApplicationProcess *Item = nullptr;

            for (int I = 0; I < Count(); I++) {
                Item = Process(I);
                if (Item->Pid() == Pid)
                    return Item;
            }

            return nullptr;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CApplication ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CApplication::CApplication(int argc, char *const *argv): CProcessManager(),
            CApplicationProcess(nullptr, this, ptMain), CCustomApplication(argc, argv) {
            GApplication = this;
            m_ProcessType = ptSingle;
        }
        //--------------------------------------------------------------------------------------------------------------

        CApplication::~CApplication() {
            GApplication = nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::SetProcessType(CProcessType Value) {
            if (m_ProcessType != Value) {
                m_ProcessType = Value;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::CreateLogFile() {
            CLogFile *LogFile;

            Log()->Level(APP_LOG_STDERR);

            u_int Level;
            for (int I = 0; I < Config()->LogFiles().Count(); ++I) {

                const CString &Key = Config()->LogFiles().Names(I);
                const CString &Value = Config()->LogFiles().Values(Key);

                Level = GetLogLevelByName(Key.c_str());
                if (Level > APP_LOG_STDERR && Level <= APP_LOG_DEBUG) {
                    Log()->AddLogFile(Value.c_str(), Level);
                }
            }

            LogFile = Log()->AddLogFile(Config()->AccessLog().c_str(), 0);
            LogFile->LogType(ltAccess);

#ifdef WITH_POSTGRESQL
            LogFile = Log()->AddLogFile(Config()->PostgresLog().c_str(), 0);
            LogFile->LogType(ltPostgres);
#endif

#ifdef _DEBUG
            const CString &Debug = Config()->LogFiles().Values(_T("debug"));
            if (Debug.IsEmpty()) {
                Log()->AddLogFile(Config()->ErrorLog().c_str(), APP_LOG_DEBUG);
            }
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::Daemonize() {

            int  fd;

            switch (fork()) {
                case -1:
                    throw EOSError(errno, "fork() failed");

                case 0:
                    break;

                default:
                    exit(0);
            }

            if (setsid() == -1) {
                throw EOSError(errno, "setsid() failed");
            }

            Daemonized(true);

            umask(0);

            if (chdir("/") == -1) {
                throw EOSError(errno, "chdir(\"/\") failed");
            }

            fd = open("/dev/null", O_RDWR);
            if (fd == -1) {
                throw EOSError(errno, "open(\"/dev/null\") failed");
            }

            if (dup2(fd, STDIN_FILENO) == -1) {
                throw EOSError(errno, "dup2(STDIN) failed");
            }

            if (dup2(fd, STDOUT_FILENO) == -1) {
                throw EOSError(errno, "dup2(STDOUT) failed");
            }

#ifndef _DEBUG
            if (dup2(fd, STDERR_FILENO) == -1) {
                throw EOSError(errno, "dup2(STDERR) failed");
            }
#endif
            if (close(fd) == -1) {
                throw EOSError(errno, "close(\"/dev/null\") failed");
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::MkDir(const CString &Dir) {
            if (!DirectoryExists(Dir.c_str()))
                if (!CreateDir(Dir.c_str(), 0700))
                    throw EOSError(errno, _T("mkdir \"%s\" failed "), Dir.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::CreateDirectories() {
            MkDir(Config()->Prefix());
            MkDir(Config()->Prefix() + _T("logs/"));
            MkDir(Config()->ConfPrefix());
            MkDir(Config()->CachePrefix());
        }
        //--------------------------------------------------------------------------------------------------------------

        pid_t CApplication::ExecNewBinary(char *const *argv) {

            char **env, *var;
            char *p;
            uint_t i, n;

            pid_t pid;

            CExecuteContext ctx;

            ZeroMemory(&ctx, sizeof(CExecuteContext));

            ctx.path = argv[0];
            ctx.name = "new binary process";
            ctx.argv = argv;

            n = 2;
            env = (char **) Environ();

            var = new char[sizeof(APP_VAR) + Server()->Bindings()->Count() * (_INT32_LEN + 1) + 2];

            p = MemCopy(var, APP_VAR "=", sizeof(APP_VAR));

            for (i = 0; i < Server()->Bindings()->Count(); i++) {
                p = ld_sprintf(p, "%ud;", Server()->Bindings()->Handles(i)->Handle());
            }

            *p = '\0';

            env[n++] = var;

            env[n] = nullptr;

#if (_DEBUG)
            {
                char **e;
                for (e = env; *e; e++) {
                    log_debug1(APP_LOG_DEBUG_CORE, Log(), 0, "env: %s", *e);
                }
            }
#endif

            ctx.envp = (char *const *) env;

            if (!RenamePidFile(false, "rename %s to %s failed "
                                      "before executing new binary process \"%s\"")) {
                return INVALID_PID;
            }

            pid = ExecProcess(&ctx);

            if (pid == INVALID_PID) {
                RenamePidFile(true, "rename() %s back to %s failed "
                                    "after an attempt to execute new binary process \"%s\"");
            }

            return pid;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::SetNewBinary(CApplicationProcess *AProcess) {
            AProcess->NewBinary(ExecNewBinary(m_os_argv));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::DoBeforeStartProcess(CApplicationProcess *AProcess) {

            AProcess->Pid(getpid());
            AProcess->Assign(this);

            if ((AProcess->Type() <= ptMaster) || (AProcess->Pid() != MainThreadID)) {
                MainThreadID = AProcess->Pid();
                SetSignalProcess(AProcess);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::DoAfterStartProcess(CApplicationProcess *AProcess) {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::StartProcess() {

            Log()->Debug(0, MSG_PROCESS_START, GetProcessName(), CmdLine().c_str());

            if (m_ProcessType == ptSingle) {
                if (Config()->Master())
                    m_ProcessType = ptMaster;
            }

            if (m_ProcessType != ptSignaller) {

                m_pPollStack = new CPollStack();
                m_pPollStack->TimeOut(Config()->TimeOut());

                CreateHTTPServer();
#ifdef WITH_POSTGRESQL
                CreatePQServer();
#endif
                SetTimerInterval(1000);
            }

            if ( Config()->Daemon() ) {
                Daemonize();

                Log()->UseStdErr(false);
                Log()->RedirectStdErr();
            }

            Start(CApplicationProcess::Create(this, m_ProcessType));
#ifdef WITH_POSTGRESQL
            // Delete PQServer
            SetPQServer(nullptr);
#endif
            // Delete HTTPServer
            SetServer(nullptr);

            delete m_pPollStack;

            Log()->Debug(0, MSG_PROCESS_STOP, GetProcessName());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplication::Run() {

            ParseCmdLine();

            if (Config()->Flags().show_version) {
                ShowVersionInfo();
                ExitRun(0);
            }

            Config()->Reload();

            if (Config()->Flags().test_config) {

                if (!FileExists(Config()->ConfFile().c_str())) {
                    Log()->Error(APP_LOG_EMERG, 0, "configuration file %s not found", Config()->ConfFile().c_str());
                    Log()->Error(APP_LOG_EMERG, 0, "configuration file %s test failed", Config()->ConfFile().c_str());
                    ExitRun(1);
                }

                if (Config()->ErrorCount() == 0) {
                    Log()->Error(APP_LOG_EMERG, 0, "configuration file %s test is successful", Config()->ConfFile().c_str());
                    ExitRun(0);
                }

                Log()->Error(APP_LOG_EMERG, 0, "configuration file %s test failed", Config()->ConfFile().c_str());
                ExitRun(1);
            }

            CreateDirectories();

            DefaultLocale.SetLocale(Config()->Locale().c_str());

            CreateLogFile();

#ifdef _DEBUG
            Log()->Error(APP_LOG_EMERG, 0, "%s version: %s (%s build)", Description().c_str(), Version().c_str(), "debug");
#else
            Log()->Error(APP_LOG_EMERG, 0, "%s version: %s (%s build)", Description().c_str(), Version().c_str(), "release");
#endif
            Log()->Error(APP_LOG_EMERG, 0, "Config file: %s", Config()->ConfFile().c_str());

            StartProcess();
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CApplicationProcess ---------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CApplicationProcess::CApplicationProcess(CCustomProcess *AParent, CApplication *AApplication,
                CProcessType AType): CModuleProcess(AType, AParent), CCollectionItem((CProcessManager *) AApplication),
                m_pApplication(AApplication) {

            m_pTimer = nullptr;
            m_TimerInterval = 0;

            m_pwd.uid = -1;
            m_pwd.gid = -1;

            m_pwd.username = nullptr;
            m_pwd.groupname = nullptr;

            m_pPollStack = nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        class CApplicationProcess *CApplicationProcess::Create(CApplication *AApplication, CProcessType AType) {

            switch (AType) {
                case ptSingle:
                    return new CProcessSingle(AApplication->SignalProcess(), AApplication);

                case ptMaster:
                    return new CProcessMaster(AApplication->SignalProcess(), AApplication);

                case ptSignaller:
                    return new CProcessSignaller(AApplication->SignalProcess(), AApplication);

                case ptNewBinary:
                    return new CProcessNewBinary(AApplication->SignalProcess(), AApplication);

                case ptWorker:
                    return new CProcessWorker(AApplication->SignalProcess(), AApplication);

                case ptHelper:
                    return new CProcessHelper(AApplication->SignalProcess(), AApplication);

                default:
                    throw ExceptionFrm(_T("Unknown process type: (%d)"), AType);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::Assign(CCustomProcess *AProcess) {
            CCustomProcess::Assign(AProcess);

            auto LProcess = dynamic_cast<CApplicationProcess *> (AProcess);

            SetServer(LProcess->Server());
#ifdef WITH_POSTGRESQL
            SetPQServer(LProcess->PQServer());
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::SetPwd() const {
            if (geteuid() == 0) {
                if (setgid(m_pwd.gid) == -1) {
                    throw Delphi::Exception::ExceptionFrm("setgid(%d) failed.", m_pwd.gid);
                }

                if (initgroups(m_pwd.username, m_pwd.gid) == -1) {
                    throw Delphi::Exception::ExceptionFrm("initgroups(%s, %d) failed.", m_pwd.username, m_pwd.gid);
                }

                if (setuid(m_pwd.uid) == -1) {
                    throw Delphi::Exception::ExceptionFrm("setuid(%d) failed.", m_pwd.username, m_pwd.gid);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::SetUser(const char *AUserName, const char *AGroupName) {
            if (m_pwd.uid == (uid_t) -1 && geteuid() == 0) {

                struct group   *grp;
                struct passwd  *pwd;

                errno = 0;
                pwd = getpwnam(AUserName);
                if (pwd == nullptr) {
                    throw Delphi::Exception::ExceptionFrm("getpwnam(\"%s\") failed.", AUserName);
                }

                errno = 0;
                grp = getgrnam(AGroupName);
                if (grp == nullptr) {
                    throw Delphi::Exception::ExceptionFrm("getgrnam(\"%s\") failed.", AGroupName);
                }

                m_pwd.username = AUserName;
                m_pwd.uid = pwd->pw_uid;

                m_pwd.groupname = AGroupName;
                m_pwd.gid = grp->gr_gid;

                SetPwd();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::SetUser(const CString &UserName, const CString &GroupName) {
            SetUser(UserName.c_str(), GroupName.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::SetLimitNoFile(uint32_t value) {
            if (value != static_cast<uint32_t>(-1)) {
                struct rlimit rlmt = { 0, 0 };

                rlmt.rlim_cur = (rlim_t) value;
                rlmt.rlim_max = (rlim_t) value;

                if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
                    throw Delphi::Exception::ExceptionFrm("setrlimit(RLIMIT_NOFILE, %i) failed.", value);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::SetTimerInterval(int Value) {
            if (m_TimerInterval != Value) {
                m_TimerInterval = Value;
                UpdateTimer();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::UpdateTimer() {
            if (Server() != nullptr) {
                if (m_pTimer == nullptr) {
                    m_pTimer = CEPollTimer::CreateTimer(CLOCK_MONOTONIC, TFD_NONBLOCK);
                    m_pTimer->AllocateTimer(Server()->EventHandlers(), m_TimerInterval, m_TimerInterval);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                    m_pTimer->OnTimer([this](auto && AHandler) { DoTimer(AHandler); });
#else
                    m_pTimer->OnTimer(std::bind(&CApplicationProcess::DoTimer, this, _1));
#endif
                } else {
                    m_pTimer->SetTimer(m_TimerInterval, m_TimerInterval);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::BeforeRun() {
            Log()->Debug(0, MSG_PROCESS_START, GetProcessName(), Application()->CmdLine().c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::AfterRun() {
            Log()->Debug(0, MSG_PROCESS_STOP, GetProcessName());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::LoadSites(CSites &Sites) {

            const CString FileName(Config()->ConfPrefix() + "sites.conf");

            auto OnIniFileParseError = [&FileName](Pointer Sender, LPCTSTR lpszSectionName, LPCTSTR lpszKeyName,
                                                   LPCTSTR lpszValue, LPCTSTR lpszDefault, int Line)
            {
                if ((lpszValue == nullptr) || (lpszValue[0] == '\0')) {
                    if ((lpszDefault == nullptr) || (lpszDefault[0] == '\0'))
                        Log()->Error(APP_LOG_EMERG, 0, ConfMsgEmpty, lpszSectionName, lpszKeyName, FileName.c_str(), Line);
                } else {
                    if ((lpszDefault == nullptr) || (lpszDefault[0] == '\0'))
                        Log()->Error(APP_LOG_EMERG, 0, ConfMsgInvalidValue, lpszSectionName, lpszKeyName, lpszValue,
                                     FileName.c_str(), Line);
                    else
                        Log()->Error(APP_LOG_EMERG, 0, ConfMsgInvalidValue _T(" - ignored and set by default: \"%s\""), lpszSectionName, lpszKeyName, lpszValue,
                                     FileName.c_str(), Line, lpszDefault);
                }
            };

            Sites.Clear();

            if (FileExists(FileName.c_str())) {
                const CString pathSites(Config()->Prefix() + "sites/");

                CIniFile HostFile(FileName.c_str());
                HostFile.OnIniFileParseError(OnIniFileParseError);

                CStringList configFiles;
                CString configFile;

                HostFile.ReadSectionValues("hosts", &configFiles);
                for (int i = 0; i < configFiles.Count(); i++) {
                    const auto& siteName = configFiles.Names(i);

                    configFile = configFiles.ValueFromIndex(i);
                    if (!path_separator(configFile.front())) {
                        configFile = pathSites + configFile;
                    }

                    if (FileExists(configFile.c_str())) {
                        int Index = Sites.AddPair(siteName, CJSON());
                        auto& Config = Sites[Index].Config;
                        Config.LoadFromFile(configFile.c_str());
                    } else {
                        Log()->Error(APP_LOG_EMERG, 0, APP_FILE_NOT_FOUND, configFile.c_str());
                    }
                }
            } else {
                Log()->Error(APP_LOG_EMERG, 0, APP_FILE_NOT_FOUND, FileName.c_str());
            }

            auto& defaultSite = Sites.Default();
            if (defaultSite.Name.IsEmpty()) {
                defaultSite.Name = _T("*");
                auto& configJson = defaultSite.Config.Object();
                configJson.AddPair(_T("hosts"), CJSONArray("[\"*\"]"));
                configJson.AddPair(_T("listen"), (int) Config()->Port());
                configJson.AddPair(_T("root"), Config()->DocRoot());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::CreateHTTPServer() {
            auto LServer = new CHTTPServer((ushort) Config()->Port());

            LServer->ServerName() = m_pApplication->Title();

            CSocketHandle* LBinding = LServer->Bindings()->Add();
            LBinding->IP(Config()->Listen().c_str());

            LoadSites(LServer->Sites());

            for (int i = 0; i < LServer->Sites().Count(); ++i) {
                const auto& Site = LServer->Sites()[i];
                const auto& listenPort = Site.Config["listen"];
                if (Site.Name == "*") {
                    if (!listenPort.IsEmpty())
                        LBinding->Port(listenPort.AsInteger());
                } else {
                    if (!listenPort.IsEmpty() && listenPort.AsInteger() != Config()->Port()) {
                        LBinding = LServer->Bindings()->Add();
                        LBinding->IP(Config()->Listen().c_str());
                        LBinding->Port(listenPort.AsInteger());
                    }
                }
            }

            LServer->PollStack(m_pPollStack);

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            LServer->OnExecute([this](auto && AConnection) { return DoExecute(AConnection); });

            LServer->OnVerbose([this](auto && Sender, auto && AConnection, auto && AFormat, auto && args) { DoVerbose(Sender, AConnection, AFormat, args); });
            LServer->OnAccessLog([this](auto && AConnection) { DoAccessLog(AConnection); });

            LServer->OnException([this](auto && AConnection, auto && AException) { DoServerException(AConnection, AException); });
            LServer->OnListenException([this](auto && AConnection, auto && AException) { DoServerListenException(AConnection, AException); });

            LServer->OnEventHandlerException([this](auto && AHandler, auto && AException) { DoServerEventHandlerException(AHandler, AException); });

            LServer->OnConnected([this](auto && Sender) { DoServerConnected(Sender); });
            LServer->OnDisconnected([this](auto && Sender) { DoServerDisconnected(Sender); });

            LServer->OnNoCommandHandler([this](auto && Sender, auto && AData, auto && AConnection) { DoNoCommandHandler(Sender, AData, AConnection); });
#else
            LServer->OnExecute(std::bind(&CApplicationProcess::DoExecute, this, _1));

            LServer->OnVerbose(std::bind(&CApplicationProcess::DoVerbose, this, _1, _2, _3, _4));
            LServer->OnAccessLog(std::bind(&CApplicationProcess::DoAccessLog, this, _1));

            LServer->OnException(std::bind(&CApplicationProcess::DoServerException, this, _1, _2));
            LServer->OnListenException(std::bind(&CApplicationProcess::DoServerListenException, this, _1, _2));

            LServer->OnEventHandlerException(std::bind(&CApplicationProcess::DoServerEventHandlerException, this, _1, _2));

            LServer->OnConnected(std::bind(&CApplicationProcess::DoServerConnected, this, _1));
            LServer->OnDisconnected(std::bind(&CApplicationProcess::DoServerDisconnected, this, _1));

            LServer->OnNoCommandHandler(std::bind(&CApplicationProcess::DoNoCommandHandler, this, _1, _2, _3));
#endif
            LServer->ActiveLevel(alBinding);

            SetServer(LServer);
        }
        //--------------------------------------------------------------------------------------------------------------

#ifdef WITH_POSTGRESQL
        void CApplicationProcess::CreatePQServer() {
            auto LPQServer = new CPQServer(Config()->PostgresPollMin(), Config()->PostgresPollMax());

            LPQServer->ConnInfo().ApplicationName() = "'" + m_pApplication->Title() + "'"; //application_name;

            LPQServer->PollStack(m_pPollStack);

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            if (Config()->PostgresNotice()) {
                //LPQServer->OnReceiver([this](auto && AConnection, auto && AResult) { DoPQReceiver(AConnection, AResult); });
                LPQServer->OnProcessor([this](auto && AConnection, auto && AMessage) { DoPQProcessor(AConnection, AMessage); });
            }

            LPQServer->OnConnectException([this](auto && AConnection, auto && AException) { DoPQConnectException(AConnection, AException); });
            LPQServer->OnServerException([this](auto && AServer, auto && AException) { DoPQServerException(AServer, AException); });

            LPQServer->OnEventHandlerException([this](auto && AHandler, auto && AException) { DoServerEventHandlerException(AHandler, AException); });

            LPQServer->OnError([this](auto && AConnection) { DoPQError(AConnection); });
            LPQServer->OnStatus([this](auto && AConnection) { DoPQStatus(AConnection); });
            LPQServer->OnPollingStatus([this](auto && AConnection) { DoPQPollingStatus(AConnection); });

            LPQServer->OnConnected([this](auto && Sender) { DoPQConnect(Sender); });
            LPQServer->OnDisconnected([this](auto && Sender) { DoPQDisconnect(Sender); });
#else
            if (Config()->PostgresNotice()) {
                //LPQServer->OnReceiver(std::bind(&CApplicationProcess::DoPQReceiver, this, _1, _2));
                LPQServer->OnProcessor(std::bind(&CApplicationProcess::DoPQProcessor, this, _1, _2));
            }

            LPQServer->OnConnectException(std::bind(&CApplicationProcess::DoPQConnectException, this, _1, _2));
            LPQServer->OnServerException(std::bind(&CApplicationProcess::DoPQServerException, this, _1, _2));

            LPQServer->OnEventHandlerException(std::bind(&CApplicationProcess::DoServerEventHandlerException, this, _1, _2));

            LPQServer->OnError(std::bind(&CApplicationProcess::DoPQError, this, _1));
            LPQServer->OnStatus(std::bind(&CApplicationProcess::DoPQStatus, this, _1));
            LPQServer->OnPollingStatus(std::bind(&CApplicationProcess::DoPQPollingStatus, this, _1));

            LPQServer->OnConnected(std::bind(&CApplicationProcess::DoPQConnect, this, _1));
            LPQServer->OnDisconnected(std::bind(&CApplicationProcess::DoPQDisconnect, this, _1));
#endif
            SetPQServer(LPQServer);
        }
        //--------------------------------------------------------------------------------------------------------------
#endif
        void CApplicationProcess::DoExitSigAlarm(uint_t AMsec) {

            sigset_t set, wset;
            struct itimerval itv = {0};

            itv.it_interval.tv_sec = 0;
            itv.it_interval.tv_usec = 0;
            itv.it_value.tv_sec = AMsec / 1000;
            itv.it_value.tv_usec = (AMsec % 1000) * 1000000;

            if (setitimer(ITIMER_REAL, &itv, nullptr) == -1) {
                Log()->Error(APP_LOG_ALERT, errno, "setitimer() failed");
            }

            /* Set up the mask of signals to temporarily block. */
            sigemptyset (&set);
            sigaddset (&set, SIGALRM);

            /* Wait for a signal to arrive. */
            sigprocmask(SIG_BLOCK, &set, &wset);

            log_debug1(APP_LOG_DEBUG_EVENT, Log(), 0, "exit alarm after %u msec", AMsec);

            while (!(sig_terminate || sig_quit))
                sigsuspend(&wset);
        }
        //--------------------------------------------------------------------------------------------------------------

        pid_t CApplicationProcess::SwapProcess(CProcessType Type, int Flag, Pointer Data) {

            CApplicationProcess *LProcess;

            if (Flag >= 0) {
                LProcess = Application()->Process(Flag);
            } else {
                LProcess = CApplicationProcess::Create(Application(), Type);
                LProcess->Data(Data);
            }

            pid_t pid = fork();

            switch (pid) {

                case -1:
                    throw EOSError(errno, _T("fork() failed while spawning \"%s process\""), LProcess->GetProcessName());

                case 0:

                    Application()->Start(LProcess);
                    exit(0);

                default:
                    break;
            }

            LProcess->Pid(pid);

            LProcess->Exited(false);

            Log()->Error(APP_LOG_NOTICE, 0, _T("start %s %P"), LProcess->GetProcessName(), LProcess->Pid());

            if (Flag >= 0) {
                return pid;
            }

            LProcess->Exiting(false);

            LProcess->Respawn(Flag == PROCESS_RESPAWN || Flag == PROCESS_JUST_RESPAWN);
            LProcess->JustSpawn(Flag == PROCESS_JUST_SPAWN || Flag == PROCESS_JUST_RESPAWN);
            LProcess->Detached(Flag == PROCESS_DETACHED);

            return pid;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::ChildProcessGetStatus() {
            int             status;
            const char     *process;
            pid_t           pid;
            int             err;
            int             i;
            bool            one;

            one = false;

            for ( ;; ) {
                pid = waitpid(-1, &status, WNOHANG);

                if (pid == 0) {
                    return;
                }

                if (pid == -1) {
                    err = errno;

                    if (err == EINTR) {
                        continue;
                    }

                    if (err == ECHILD && one) {
                        return;
                    }

                    /*
                     * Solaris always calls the signal handler for each exited process
                     * despite waitpid() may be already called for this process.
                     *
                     * When several processes exit at the same time FreeBSD may
                     * erroneously call the signal handler for exited process
                     * despite waitpid() may be already called for this process.
                     */

                    if (err == ECHILD) {
                        Log()->Error(APP_LOG_INFO, err, "waitpid() failed");
                        return;
                    }

                    Log()->Error(APP_LOG_ALERT, err, "waitpid() failed");
                    return;
                }

                one = true;
                process = "unknown process";

                CApplicationProcess *LProcess = this;
                for (i = 0; i < Application()->ProcessCount(); ++i) {
                    LProcess = Application()->Process(i);
                    if (LProcess->Pid() == pid) {
                        LProcess->Status(status);
                        LProcess->Exited(true);
                        process = LProcess->GetProcessName();
                        break;
                    }
                }

                if (WTERMSIG(status)) {
#ifdef WCOREDUMP
                    Log()->Error(APP_LOG_ALERT, 0,
                                 "%s %P exited on signal %d%s",
                                 process, pid, WTERMSIG(status),
                                 WCOREDUMP(status) ? " (core dumped)" : "");
#else
                    Log()->Error(APP_LOG_ALERT, 0,
                          "%s %P exited on signal %d",
                          process, pid, WTERMSIG(status));
#endif

                } else {
                    Log()->Error(APP_LOG_NOTICE, 0,
                                 "%s %P exited with code %d",
                                 process, pid, WEXITSTATUS(status));
                }

                if (WEXITSTATUS(status) == 2 && LProcess->Respawn()) {
                    Log()->Error(APP_LOG_ALERT, 0,
                                 "%s %P exited with fatal code %d "
                                 "and cannot be respawned",
                                 process, pid, WEXITSTATUS(status));
                    LProcess->Respawn(false);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        pid_t CApplicationProcess::ExecProcess(CExecuteContext *AContext)
        {
            return SwapProcess(ptNewBinary, PROCESS_DETACHED, AContext);
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CApplicationProcess::RenamePidFile(bool Back, LPCTSTR lpszErrorMessage) {
            int Result;

            TCHAR szOldPid[PATH_MAX + 1] = {0};

            LPCTSTR lpszPid;
            LPCTSTR lpszOldPid;

            lpszPid = Config()->PidFile().c_str();
            Config()->PidFile().Copy(szOldPid, PATH_MAX);
            lpszOldPid = ChangeFileExt(szOldPid, Config()->PidFile().c_str(), APP_OLDPID_EXT);

            if (Back) {
                Result = ::rename(lpszOldPid, lpszPid);
                if (Result == INVALID_FILE)
                    Log()->Error(APP_LOG_ALERT, errno, lpszErrorMessage, lpszOldPid, lpszPid, Application()->argv()[0].c_str());
            } else {
                Result = ::rename(lpszPid, lpszOldPid);
                if (Result == INVALID_FILE)
                    Log()->Error(APP_LOG_ALERT, errno, lpszErrorMessage, lpszPid, lpszOldPid, Application()->argv()[0].c_str());
            }

            return (Result != INVALID_FILE);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::CreatePidFile() {

            size_t len;
            char pid[_INT64_LEN + 2];
            int create = Config()->Flags().test_config ? FILE_CREATE_OR_OPEN : FILE_TRUNCATE;

            if (Daemonized()) {

                CFile File(Config()->PidFile().c_str(), FILE_RDWR | create);

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
                File.setOnFilerError([this](auto && Sender, auto && Error, auto && lpFormat, auto && args) { OnFilerError(Sender, Error, lpFormat, args); });
#else
                File.setOnFilerError(std::bind(&CApplicationProcess::OnFilerError, this, _1, _2, _3, _4));
#endif
                File.Open();

                if (!Config()->Flags().test_config) {
                    len = ld_snprintf(pid, _INT64_LEN + 2, "%P%N", Pid()) - pid;
                    File.Write(pid, len, 0);
                }

                File.Close();
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::DeletePidFile() {

            if (Daemonized()) {
                LPCTSTR lpszPid = Config()->PidFile().c_str();

                if (unlink(lpszPid) == FILE_ERROR) {
                    Log()->Error(APP_LOG_ALERT, errno, _T("could not delete pid file: \"%s\" error: "), lpszPid);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::ServerStart() {
            Server()->ActiveLevel(alActive);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::ServerStop() {
            Server()->ActiveLevel(alBinding);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::ServerShutDown() {
            Server()->ActiveLevel(alShutDown);
        }
        //--------------------------------------------------------------------------------------------------------------
#ifdef WITH_POSTGRESQL
        void CApplicationProcess::PQServerStart() {
            if (Config()->PostgresConnect()) {
                PQServer()->ConnInfo().SetParameters(Config()->PostgresConnInfo());
                PQServer()->Active(true);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CApplicationProcess::PQServerStop() {
            PQServer()->Active(false);
        }
        //--------------------------------------------------------------------------------------------------------------
#endif
        void CApplicationProcess::OnFilerError(Pointer Sender, int Error, LPCTSTR lpFormat, va_list args) {
            Log()->Error(APP_LOG_ALERT, Error, lpFormat, args);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessSingle --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CProcessSingle::DoExit() {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessSingle::Reload() {
#ifdef WITH_POSTGRESQL
            PQServerStop();
#endif
            ServerStop();

            Config()->Reload();

            ServerStart();
#ifdef WITH_POSTGRESQL
            PQServerStart();
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessSingle::BeforeRun() {
            Application()->Header(Application()->Name() + ": single process " + Application()->CmdLine());

            Log()->Debug(0, MSG_PROCESS_START, GetProcessName(), Application()->Header().c_str());

            InitSignals();

            SetLimitNoFile(Config()->LimitNoFile());

            ServerStart();
#ifdef WITH_POSTGRESQL
            PQServerStart();
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessSingle::AfterRun() {
#ifdef WITH_POSTGRESQL
            PQServerStop();
#endif
            ServerStop();

            CApplicationProcess::AfterRun();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessSingle::Run() {

            while (!(sig_terminate || sig_quit)) {

                log_debug0(APP_LOG_DEBUG_EVENT, Log(), 0, "single cycle");

                try
                {
                    Server()->Wait();
                }
                catch (std::exception& e)
                {
                    Log()->Error(APP_LOG_EMERG, 0, e.what());
                }

                if (sig_reconfigure) {
                    sig_reconfigure = 0;
                    Log()->Error(APP_LOG_NOTICE, 0, "reconfiguring");

                    Reload();
                }

                if (sig_reopen) {
                    sig_reopen = 0;
                    Log()->Error(APP_LOG_NOTICE, 0, "reopening logs");
                    Log()->RedirectStdErr();
                }
            }

            Log()->Error(APP_LOG_NOTICE, 0, "exiting single process");
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessMaster --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::BeforeRun() {
            Application()->Header(Application()->Name() + ": master process " + Application()->CmdLine());

            Log()->Debug(0, MSG_PROCESS_START, GetProcessName(), Application()->Header().c_str());

            InitSignals();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::AfterRun() {
            CApplicationProcess::AfterRun();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::StartProcess(CProcessType Type, int Flag) {
            if (Type == ptWorker) {
                for (int I = 0; I < Config()->Workers(); ++I) {
                    SwapProcess(Type, Flag);
                }
            } else {
                SwapProcess(Type, Flag);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::StartProcesses(int Flag) {
            StartProcess(ptWorker, Flag);
            //StartProcess(ptHelper, Flag);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::SignalToProcess(CProcessType Type, int SigNo) {
            int err;
            CCustomProcess *LProcess;

            for (int i = 0; i < Application()->ProcessCount(); ++i) {
                LProcess = Application()->Process(i);

                if (LProcess->Type() != Type)
                    continue;

                log_debug7(APP_LOG_DEBUG_EVENT, Log(), 0,
                           "signal child: %i %P e:%d t:%d d:%d r:%d j:%d",
                           i,
                           LProcess->Pid(),
                           LProcess->Exiting() ? 1 : 0,
                           LProcess->Exited() ? 1 : 0,
                           LProcess->Detached() ? 1 : 0,
                           LProcess->Respawn() ? 1 : 0,
                           LProcess->JustSpawn() ? 1 : 0);

                if (LProcess->Detached()) {
                    continue;
                }

                if (LProcess->JustSpawn()) {
                    LProcess->JustSpawn(false);
                    continue;
                }

                if (LProcess->Exiting() && (SigNo == signal_value(SIG_SHUTDOWN_SIGNAL))) {
                    continue;
                }

                log_debug2(APP_LOG_DEBUG_CORE, Log(), 0, "kill (%P, %d)", LProcess->Pid(), SigNo);

                if (kill(LProcess->Pid(), SigNo) == -1) {
                    err = errno;
                    Log()->Error(APP_LOG_ALERT, err, "kill(%P, %d) failed", LProcess->Pid(), SigNo);

                    if (err == ESRCH) {
                        LProcess->Exited(true);
                        LProcess->Exiting(false);
                        sig_reap = 1;
                    }

                    continue;
                }

                if (SigNo != signal_value(SIG_REOPEN_SIGNAL)) {
                    LProcess->Exiting(true);
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::SignalToProcesses(int SigNo) {
            //SignalToProcess(ptHelper, SigNo);
            SignalToProcess(ptWorker, SigNo);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::DoExit() {
            DeletePidFile();

            Log()->Error(APP_LOG_NOTICE, 0, _T("exiting master process"));
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CProcessMaster::ReapChildren() {

            bool live = false;
            CApplicationProcess *LProcess;

            for (int i = 0; i < Application()->ProcessCount(); ++i) {

                LProcess = Application()->Process(i);

                if (LProcess->Type() == ptMain)
                    continue;

                log_debug7(APP_LOG_DEBUG_EVENT, Log(), 0,
                           "reap child: %i %P e:%d t:%d d:%d r:%d j:%d",
                           i,
                           LProcess->Pid(),
                           LProcess->Exiting() ? 1 : 0,
                           LProcess->Exited() ? 1 : 0,
                           LProcess->Detached() ? 1 : 0,
                           LProcess->Respawn() ? 1 : 0,
                           LProcess->JustSpawn() ? 1 : 0);

                if (LProcess->Exited()) {

                    if (LProcess->Respawn() && !LProcess->Exiting() && !(sig_terminate || sig_quit)) {

                        if ((LProcess->Type() == ptWorker) || (LProcess->Type() == ptHelper)) {
                            if (SwapProcess(LProcess->Type(), i) == -1) {
                                Log()->Error(APP_LOG_ALERT, 0, "could not respawn %s", LProcess->GetProcessName());
                                continue;
                            }
                        }

                        live = true;

                        continue;
                    }

                    if (LProcess->Pid() == NewBinary()) {

                        RenamePidFile(true, "rename() %s back to %s failed after the new binary process \"%s\" exited");

                        NewBinary(0);

                        if (sig_noaccepting) {
                            sig_restart = 1;
                            sig_noaccepting = 0;
                        }
                    }

                    Application()->DeleteProcess(i);

                } else if (LProcess->Exiting() || !LProcess->Detached()) {
                    live = true;
                }

            }

            return live;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessMaster::Run() {

            int sigio;
            sigset_t set, wset;
            struct itimerval itv = {};
            bool live;
            uint_t delay;

            CreatePidFile();

            SigProcMask(SIG_BLOCK, SigAddSet(&set), &wset);

            StartProcesses(PROCESS_RESPAWN);

            NewBinary(0);

            delay = 0;
            sigio = 0;

            CApplicationProcess *LProcess;
            for (int i = 0; i < Application()->ProcessCount(); ++i) {
                LProcess = Application()->Process(i);

                log_debug9(APP_LOG_DEBUG_EVENT, Log(), 0,
                           "process (%s)\t: %P - %i %P e:%d t:%d d:%d r:%d j:%d",
                           LProcess->GetProcessName(),
                           Pid(),
                           i,
                           LProcess->Pid(),
                           LProcess->Exiting() ? 1 : 0,
                           LProcess->Exited() ? 1 : 0,
                           LProcess->Detached() ? 1 : 0,
                           LProcess->Respawn() ? 1 : 0,
                           LProcess->JustSpawn() ? 1 : 0);
            }

            live = true;
            while (!(live && (sig_terminate || sig_quit))) {

                if (delay) {
                    if (sig_sigalrm) {
                        sigio = 0;
                        delay *= 2;
                        sig_sigalrm = 0;
                    }

                    log_debug1(APP_LOG_DEBUG_EVENT, Log(), 0, "termination cycle: %M", delay);

                    itv.it_interval.tv_sec = 0;
                    itv.it_interval.tv_usec = 0;
                    itv.it_value.tv_sec = delay / 1000;
                    itv.it_value.tv_usec = (delay % 1000 ) * 1000;

                    if (setitimer(ITIMER_REAL, &itv, nullptr) == -1) {
                        Log()->Error(APP_LOG_ALERT, errno, "setitimer() failed");
                    }
                }

                log_debug0(APP_LOG_DEBUG_EVENT, Log(), 0, "sigsuspend");

                sigsuspend(&wset);

                log_debug1(APP_LOG_DEBUG_EVENT, Log(), 0, "wake up, sigio %i", sigio);

                if (sig_reap) {
                    sig_reap = 0;
                    log_debug0(APP_LOG_DEBUG_EVENT, Log(), 0, "reap children");

                    live = ReapChildren();
                }

                if (sig_terminate) {
                    if (delay == 0) {
                        delay = 50;
                    }

                    if (sigio) {
                        sigio--;
                        continue;
                    }

                    sigio = Config()->Workers();

                    if (delay > 1000) {
                        SignalToProcesses(SIGKILL);
                    } else {
                        SignalToProcesses(signal_value(SIG_TERMINATE_SIGNAL));
                    }

                    continue;
                }

                if (sig_quit) {
                    SignalToProcesses(signal_value(SIG_SHUTDOWN_SIGNAL));
                    continue;
                }

                if (sig_reconfigure) {
                    sig_reconfigure = 0;

                    if (NewBinary() > 0) {
                        StartProcesses(PROCESS_RESPAWN);
                        sig_noaccepting = 0;

                        continue;
                    }

                    Log()->Error(APP_LOG_NOTICE, 0, "reconfiguring");

                    StartProcesses(PROCESS_JUST_RESPAWN);

                    /* allow new processes to start */
                    usleep(100 * 1000);

                    live = true;

                    SignalToProcesses(signal_value(SIG_SHUTDOWN_SIGNAL));
                }

                if (sig_restart) {
                    sig_restart = 0;
                    StartProcesses(PROCESS_RESPAWN);
                    live = true;
                }

                if (sig_reopen) {
                    sig_reopen = 0;

                    Log()->Error(APP_LOG_NOTICE, 0, "reopening logs");
                    // TODO: reopen files;

                    SignalToProcesses(signal_value(SIG_REOPEN_SIGNAL));
                }

                if (sig_change_binary) {
                    sig_change_binary = 0;
                    Log()->Error(APP_LOG_NOTICE, 0, "changing binary");
                    Application()->SetNewBinary(this);
                }

                if (sig_noaccept) {
                    sig_noaccept = 0;
                    sig_noaccepting = 1;
                    SignalToProcesses(signal_value(SIG_SHUTDOWN_SIGNAL));
                }
            }

            DoExit();
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessSignaller -----------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CProcessSignaller::SignalToProcess(pid_t pid) {

            CSignal *Signal;
            LPCTSTR lpszSignal = Config()->Signal().c_str();

            for (int I = 0; I < SignalsCount(); ++I) {
                Signal = Signals(I);
                if (SameText(lpszSignal, Signal->Name())) {
                    if (kill(pid, Signal->SigNo()) == -1) {
                        Log()->Error(APP_LOG_ALERT, errno, _T("kill(%P, %d) failed"), pid, Signal->SigNo());
                    }
                    break;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessSignaller::Run() {

            ssize_t       n;
            pid_t         pid;
            char          buf[_INT64_LEN + 2] = {0};

            LPCTSTR lpszPid = Config()->PidFile().c_str();

            CFile File(lpszPid, FILE_RDONLY | FILE_OPEN);

#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            File.setOnFilerError([this](auto && Sender, auto && Error, auto && lpFormat, auto && args) { OnFilerError(Sender, Error, lpFormat, args); });
#else
            File.setOnFilerError(std::bind(&CApplicationProcess::OnFilerError, this, _1, _2, _3, _4));
#endif
            File.Open();

            n = File.Read(buf, _INT64_LEN + 2, 0);

            File.Close();

            while (n-- && (buf[n] == '\r' || buf[n] == '\n')) { buf[n] = '\0'; }

            pid = (pid_t) strtol(buf, nullptr, 0);

            if (pid == (pid_t) 0) {
                throw ExceptionFrm(_T("invalid PID number \"%*s\" in \"%s\""), n, buf, lpszPid);
            }

            SignalToProcess(pid);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessNewBinary -----------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CProcessNewBinary::Run() {
            auto LContext = (CExecuteContext *) Data();
            ExecuteProcess(LContext);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessWorker --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CProcessWorker::DoExit() {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessWorker::BeforeRun() {
            sigset_t set;

            Application()->Header(Application()->Name() + ": worker process");

            Log()->Debug(0, MSG_PROCESS_START, GetProcessName(), Application()->Header().c_str());

            InitSignals();

            Config()->Reload();

            SetLimitNoFile(Config()->LimitNoFile());

            SetUser(Config()->User().c_str(), Config()->Group().c_str());

            ServerStart();
#ifdef WITH_POSTGRESQL
            PQServerStart();
#endif
            SigProcMask(SIG_UNBLOCK, SigAddSet(&set));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessWorker::AfterRun() {
#ifdef WITH_POSTGRESQL
            PQServerStop();
#endif
            ServerStop();
            CApplicationProcess::AfterRun();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessWorker::Run() {

            while (!sig_exiting) {

                log_debug0(APP_LOG_DEBUG_EVENT, Log(), 0, "worker cycle");

                try
                {
                    Server()->Wait();
                }
                catch (std::exception& e)
                {
                    Log()->Error(APP_LOG_EMERG, 0, e.what());
                }

                if (sig_terminate || sig_quit) {
                    if (sig_quit) {
                        sig_quit = 0;
                        Log()->Error(APP_LOG_NOTICE, 0, "gracefully shutting down");
                        Application()->Header("worker process is shutting down");
                    }

                    Log()->Error(APP_LOG_NOTICE, 0, "exiting worker process");

                    if (!sig_exiting) {
                        sig_exiting = 1;
                    }
                }

                if (sig_reopen) {
                    sig_reopen = 0;
                    Log()->Error(APP_LOG_NOTICE, 0, "reopening logs");
                    //ReopenFiles(-1);
                }
            }

            Log()->Error(APP_LOG_NOTICE, 0, "stop worker process");
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessHelper --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CProcessHelper::BeforeRun() {
            CApplicationProcess::BeforeRun();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessHelper::AfterRun() {
            CApplicationProcess::AfterRun();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessHelper::DoExit() {

        }
        //--------------------------------------------------------------------------------------------------------------

        void CProcessHelper::Run() {

        }

        //--------------------------------------------------------------------------------------------------------------
    }
}

}
