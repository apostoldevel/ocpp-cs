/*++

Library name:

  apostol-core

Module Name:

  Application.hpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_APPLICATION_HPP
#define APOSTOL_APPLICATION_HPP
//----------------------------------------------------------------------------------------------------------------------

#define MSG_PROCESS_START _T("start process: (%s) - %s")
#define MSG_PROCESS_STOP _T("stop process: (%s)")
//----------------------------------------------------------------------------------------------------------------------

#define ExitRun(Status) {   \
  ExitCode(Status);         \
  return;                   \
}                           \
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Application {

        class CApplication;
        //--------------------------------------------------------------------------------------------------------------

        extern CApplication *Application;

        //--------------------------------------------------------------------------------------------------------------

        //-- CCustomApplication ----------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CCustomApplication: public CObject {
        private:

            int                 m_argc;
            CStringList         m_argv;

            CString             m_cmdline;

            CString             m_name;
            CString             m_description;
            CString             m_version;
            CString             m_title;

            CString             m_header;

        protected:

            int                 m_exitcode;

            char              **m_os_argv;
            char              **m_os_environ;
            char               *m_os_argv_last;
            char               *m_environ;

            void Initialize();
            void SetEnviron();

            void SetHeader(LPCTSTR Value);

        public:

            CCustomApplication(int argc, char *const *argv);

            ~CCustomApplication() override;

            char *const *Environ() { return &m_environ; };

            int ExitCode() { return m_exitcode; };
            void ExitCode(int Status) { m_exitcode = Status; };

            const CStringList &argv() { return m_argv; };

            int argc() { return m_argc; };

            char *const *os_argv() { return m_os_argv; };

            const CString& CmdLine() { return m_cmdline; };

            CString& Name() { return m_name; };
            const CString& Name() const { return m_name; };

            CString& Description() { return m_description; };
            const CString& Description() const { return m_description; };

            CString& Version() { return m_version; };
            const CString& Version() const { return m_version; };

            CString& Title() { return m_title; };
            const CString& Title() const { return m_title; };

            CString& Header() { return m_header; };

            // Replace command name
            void Header(const CString& Value) { SetHeader(Value.c_str()); };
            void Header(LPCTSTR Value) { SetHeader(Value); };

        }; // class CCustomApplication

        //--------------------------------------------------------------------------------------------------------------

        //-- CApplicationProcess ---------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CApplicationProcess: public CModuleProcess, public CCollectionItem {

        private:

            CApplication *m_pApplication;

            typedef struct pwd_t {
                const char *username;
                const char *groupname;
                uid_t uid;
                gid_t gid;
            } CPasswd;

            CPasswd m_pwd;

            void SetPwd();

        protected:

            CPollStack *m_PollStack;

            void CreateHTTPServer();
#ifdef WITH_POSTGRESQL
            void CreatePQServer();
#endif
            void DoExitSigAlarm(uint_t AMsec);

            pid_t SwapProcess(CProcessType Type, int Flag, Pointer Data = nullptr);

            pid_t ExecProcess(CExecuteContext *AContext);

            void ChildProcessGetStatus() override;

        public:

            explicit CApplicationProcess(CCustomProcess* AParent, CApplication *AApplication, CProcessType AType);

            ~CApplicationProcess() override = default;

            static class CApplicationProcess *Create(CApplication *AApplication, CProcessType AType);

            void Assign(CCustomProcess *AProcess) override;

            CApplication *Application() { return m_pApplication; };

            bool RenamePidFile(bool Back, LPCTSTR lpszErrorMessage);

            void BeforeRun() override;
            void AfterRun() override;

            void CreatePidFile();
            void DeletePidFile();

            void SetUser(const char *AUserName, const char *AGroupName);

            void ServerStart();
            void ServerStop();
            void ServerShutDown();
#ifdef WITH_POSTGRESQL
            void PQServerStart();
            void PQServerStop();
#endif
            void OnFilerError(Pointer Sender, int Error, LPCTSTR lpFormat, va_list args);

        }; // class CApplicationProcess

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessManager -------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CProcessManager: public CCollection {
            typedef CCollection inherited;

        protected:

            virtual void DoBeforeStartProcess(CApplicationProcess *AProcess) abstract;
            virtual void DoAfterStartProcess(CApplicationProcess *AProcess) abstract;

        public:

            explicit CProcessManager(): CCollection(this) {};

            void Start(CApplicationProcess *AProcess);
            void Stop(int Index);
            void StopAll();

            int ProcessCount() { return inherited::Count(); };
            void DeleteProcess(int Index) { inherited::Delete(Index); };

            CApplicationProcess *FindProcessById(pid_t Pid);

            CApplicationProcess *Process(int Index) { return (CApplicationProcess *) inherited::GetItem(Index); }
            void Process(int Index, CApplicationProcess *Value) { inherited::SetItem(Index, (CCollectionItem *) Value); }
        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CApplication ----------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CApplication: public CProcessManager, public CCustomApplication, public CApplicationProcess {
            friend CApostolModule;

        private:

            CProcessType m_ProcessType;

            void CreateLogFile();
            void Daemonize();

            void StartProcess();

        protected:

            void MkDir(const CString &Dir);
            void CreateDirectories();

            void DoBeforeStartProcess(CApplicationProcess *AProcess) override;
            void DoAfterStartProcess(CApplicationProcess *AProcess) override;

            void SetProcessType(CProcessType Value);

            virtual void ParseCmdLine() abstract;
            virtual void ShowVersionInfo() abstract;

        public:

            CApplication(int argc, char *const *argv);

            inline void Destroy() override { delete this; };

            ~CApplication() override = default;

            pid_t ExecNewBinary(char *const *argv);

            void SetNewBinary(CApplicationProcess *AProcess);

            CProcessType ProcessType() { return m_ProcessType; };
            void ProcessType(CProcessType Value) { SetProcessType(Value); };

            void Run() override;

        }; // class CApplication

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessSingle --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CProcessSingle: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        protected:

            void BeforeRun() override;
            void AfterRun() override;

            void Reload();

            void DoExit();

        public:

            explicit CProcessSingle(CCustomProcess* AParent, CApplication *AApplication):
                inherited(AParent, AApplication, ptSingle) {
            };

            ~CProcessSingle() override = default;

            void Run() override;

        };
        //--------------------------------------------------------------------------------------------------------------

        class CProcessMaster: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        protected:

            void BeforeRun() override;
            void AfterRun() override;

            void DoExit();

            bool ReapChildren();

            void StartProcess(CProcessType Type, int Flag);
            void StartProcesses(int Flag);

            void SignalToProcess(CProcessType Type, int Signo);
            void SignalToProcesses(int Signo);

        public:

            explicit CProcessMaster(CCustomProcess* AParent, CApplication *AApplication):
                inherited(AParent, AApplication, ptMaster) {
            };

            ~CProcessMaster() override = default;

            void Run() override;

        };
        //--------------------------------------------------------------------------------------------------------------

        class CProcessSignaller: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        private:

            void SignalToProcess(pid_t pid);

        public:

            explicit CProcessSignaller(CCustomProcess* AParent, CApplication *AApplication):
                inherited(AParent, AApplication, ptSignaller) {
            };

            ~CProcessSignaller() override = default;

            void Run() override;
        };
        //--------------------------------------------------------------------------------------------------------------

        class CProcessNewBinary: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        public:

            explicit CProcessNewBinary(CCustomProcess* AParent, CApplication *AApplication):
                    inherited(AParent, AApplication, ptNewBinary) {
            };

            ~CProcessNewBinary() override = default;

            void Run() override;
        };
        //--------------------------------------------------------------------------------------------------------------

        class CProcessWorker: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        private:

            void BeforeRun() override;
            void AfterRun() override;

            void DoExit();

        public:

            explicit CProcessWorker(CCustomProcess* AParent, CApplication *AApplication):
                inherited(AParent, AApplication, ptWorker) {
            };

            ~CProcessWorker() override = default;

            void Run() override;
        };
        //--------------------------------------------------------------------------------------------------------------

        class CProcessHelper: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        private:

            void BeforeRun() override;
            void AfterRun() override;

            void DoExit();

        public:

            explicit CProcessHelper(CCustomProcess* AParent, CApplication *AApplication):
                    inherited(AParent, AApplication, ptHelper) {
            };

            ~CProcessHelper() override = default;

            void Run() override;
        };
        //--------------------------------------------------------------------------------------------------------------

    }
}

using namespace Apostol::Application;
}
#endif //APOSTOL_APPLICATION_HPP
