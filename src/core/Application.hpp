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

        extern CApplication *GApplication;

        //--------------------------------------------------------------------------------------------------------------

        //-- CApplicationProcess ---------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CApplicationProcess: public CModuleProcess, public CCollectionItem {
        private:

            CApplication *m_pApplication;

            struct pwd_t {
                const char *username;
                const char *groupname;
                uid_t uid;
                gid_t gid;

                pwd_t () {
                    username = nullptr;
                    groupname = nullptr;
                    uid = (uid_t) -1;
                    gid = (gid_t) -1;
                }

            } m_pwd;

            void SetPwd() const;

        protected:

            CEPollTimer *m_pTimer;

            CPollStack *m_pPollStack;

            int m_TimerInterval;

            void CreateHTTPServer();
#ifdef WITH_POSTGRESQL
            void CreatePQServer();
#endif
            void DoExitSigAlarm(uint_t AMsec);

            pid_t SwapProcess(CProcessType Type, int Flag, Pointer Data = nullptr);

            pid_t ExecProcess(CExecuteContext *AContext);

            void ChildProcessGetStatus() override;

            void SetTimerInterval(int Value);

            virtual void UpdateTimer();

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
            void SetUser(const CString& UserName, const CString& GroupName);

            static void SetLimitNoFile(uint32_t value);

            static void LoadSites(CSites &Sites);

            int TimerInterval() const { return m_TimerInterval; }
            void TimerInterval(int Value) { SetTimerInterval(Value); }

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

        protected:

            CProcessType m_ProcessType;

            static void CreateLogFile();
            void Daemonize();

            static void CreateDirectories();

            void DoBeforeStartProcess(CApplicationProcess *AProcess) override;
            void DoAfterStartProcess(CApplicationProcess *AProcess) override;

            void SetProcessType(CProcessType Value);

            virtual void ParseCmdLine() abstract;
            virtual void ShowVersionInfo() abstract;

            virtual void StartProcess();

        public:

            CApplication(int argc, char *const *argv);

            ~CApplication() override;

            inline void Destroy() override { delete this; };

            pid_t ExecNewBinary(char *const *argv);

            void SetNewBinary(CApplicationProcess *AProcess);

            CProcessType ProcessType() { return m_ProcessType; };
            void ProcessType(CProcessType Value) { SetProcessType(Value); };

            void Run() override;

            static void MkDir(const CString &Dir);

        }; // class CApplication

        //--------------------------------------------------------------------------------------------------------------

        //-- CProcessSingle --------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        class CProcessSingle: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        protected:

            void BeforeRun() override;
            void AfterRun() override;

        protected:

            void DoExit();

        public:

            CProcessSingle(CCustomProcess *AParent, CApplication *AApplication):
                    inherited(AParent, AApplication, ptSingle) {
            };

            ~CProcessSingle() override = default;

            void Run() override;

            void Reload();

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

            void SignalToProcess(CProcessType Type, int SigNo);
            void SignalToProcesses(int SigNo);

        public:

            CProcessMaster(CCustomProcess* AParent, CApplication *AApplication):
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

            CProcessSignaller(CCustomProcess* AParent, CApplication *AApplication):
                    inherited(AParent, AApplication, ptSignaller) {
            };

            ~CProcessSignaller() override = default;

            void Run() override;
        };
        //--------------------------------------------------------------------------------------------------------------

        class CProcessNewBinary: public CApplicationProcess {
            typedef CApplicationProcess inherited;

        public:

            CProcessNewBinary(CCustomProcess* AParent, CApplication *AApplication):
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

        protected:

            void DoExit();

        public:

            CProcessWorker(CCustomProcess *AParent, CApplication *AApplication):
                    inherited(AParent, AApplication, ptWorker) {
            }

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

            CProcessHelper(CCustomProcess* AParent, CApplication *AApplication):
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
