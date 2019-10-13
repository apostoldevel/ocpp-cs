/*++

Library name:

  apostol-core

Module Name:

  Config.cpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "Core.hpp"
#include "Config.hpp"
//----------------------------------------------------------------------------------------------------------------------

#define ConfMsgInvalidKey   _T("section \"%s\" invalid key \"%s\" in %s")
#define ConfMsgInvalidValue _T("section \"%s\" key \"%s\" invalid value \"%s\" in %s:%d")
#define ConfMsgEmpty        _T("section \"%s\" key \"%s\" value is empty in %s:%d")

extern "C++" {

namespace Apostol {

    namespace Config {

        static unsigned long GConfigCount = 0;
        //--------------------------------------------------------------------------------------------------------------

        CConfig *GConfig = nullptr;
        //--------------------------------------------------------------------------------------------------------------

        inline void AddConfig() {
            if (GConfigCount == 0) {
                GConfig = CConfig::Create();
            }

            GConfigCount++;
        };
        //--------------------------------------------------------------------------------------------------------------

        inline void RemoveConfig() {
            GConfigCount--;

            if (GConfigCount == 0) {
                GConfig->Destroy();
                GConfig = nullptr;
            }
        };
        //--------------------------------------------------------------------------------------------------------------

        inline char * GetCwd() {

            char  S[PATH_MAX];
            char *P = getcwd(S, PATH_MAX);

            if (P == nullptr) {
                throw EOSError(errno, _T("[emerg]: getcwd() failed: "));
            }

            return P;
        }
        //--------------------------------------------------------------------------------------------------------------

        inline char * GetHomeDir(uid_t uid) {
            struct passwd  *pwd;

            errno = 0;
            pwd = getpwuid(uid);
            if (pwd == nullptr) {
                throw Delphi::Exception::ExceptionFrm("getpwuid(\"%d\") failed.", uid);
            }

            return pwd->pw_dir;
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CCustomConfig ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CCustomConfig::CCustomConfig(): CObject() {
            m_pCommands = nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        CCustomConfig::~CCustomConfig() {
            Clear();
            delete m_pCommands;
        }
        //--------------------------------------------------------------------------------------------------------------

        CConfigCommand *CCustomConfig::Get(int Index) {
            if (Assigned(m_pCommands))
                return (CConfigCommand *) m_pCommands->Objects(Index);

            return nullptr;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomConfig::Put(int Index, CConfigCommand *ACommand) {
            if (!Assigned(m_pCommands))
                m_pCommands = new CStringList(true);

            m_pCommands->InsertObject(Index, ACommand->Ident(), ACommand);
        }
        //--------------------------------------------------------------------------------------------------------------

        int CCustomConfig::GetCount() {
            if (Assigned(m_pCommands))
                return m_pCommands->Count();
            return 0;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomConfig::Clear() {
            if (Assigned(m_pCommands))
                m_pCommands->Clear();
        }
        //--------------------------------------------------------------------------------------------------------------

        int CCustomConfig::Add(CConfigCommand *ACommand) {
            int Index = GetCount();
            Put(Index, ACommand);
            return Index;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCustomConfig::Delete(int Index) {
            if (Assigned(m_pCommands))
                m_pCommands->Delete(Index);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CConfigComponent ------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CConfigComponent::CConfigComponent() {
            AddConfig();
        }
        //--------------------------------------------------------------------------------------------------------------

        CConfigComponent::~CConfigComponent() {
            RemoveConfig();
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CConfig ---------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CConfig::CConfig(CLog *ALog): CCustomConfig() {
            m_pLog = ALog;
            m_uErrorCount = 0;

            m_nWorkers = 0;
            m_nPort = 0;

            m_nTimeOut = INFINITE;
            m_nConnectTimeOut = 0;

            m_fMaster = false;
            m_fDaemon = false;

            m_Flags = {false, false, false, false};
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetUser(LPCTSTR AValue) {
            if (m_sUser != AValue) {
                if (AValue != nullptr) {
                    m_sUser = AValue;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetGroup(LPCTSTR AValue) {
            if (m_sGroup != AValue) {
                if (AValue != nullptr) {
                    m_sGroup = AValue;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetListen(LPCTSTR AValue) {
            if (m_sListen != AValue) {
                if (AValue != nullptr) {
                    m_sListen = AValue;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetPrefix(LPCTSTR AValue) {
            if (m_sPrefix != AValue) {

                if (AValue != nullptr) {
                    m_sPrefix = AValue;
                } else {
                    m_sPrefix = GetCwd();
                }

                if (m_sPrefix.front() == '~') {
                    CString S = m_sPrefix.SubString(1);
                    m_sPrefix = GetHomeDir(getuid());
                    m_sPrefix << S;
                }

                if (!path_separator(m_sPrefix.back())) {
                    m_sPrefix += '/';
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetConfPrefix(LPCTSTR AValue) {
            if (m_sConfPrefix != AValue) {

                if (AValue != nullptr)
                    m_sConfPrefix = AValue;
                else
                    m_sConfPrefix = m_sPrefix;

                if (m_sConfPrefix.empty()) {
                    m_sConfPrefix = GetCwd();
                }

                if (!path_separator(m_sConfPrefix.front())) {
                    m_sConfPrefix = m_sPrefix + m_sConfPrefix;
                }

                if (!path_separator(m_sConfPrefix.back())) {
                    m_sConfPrefix += '/';
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetCachePrefix(LPCTSTR AValue) {
            if (m_sCachePrefix != AValue) {

                if (AValue != nullptr)
                    m_sCachePrefix = AValue;
                else
                    m_sCachePrefix = m_sPrefix;

                if (m_sCachePrefix.empty()) {
                    m_sCachePrefix = GetCwd();
                }

                if (!path_separator(m_sCachePrefix.front())) {
                    m_sCachePrefix = m_sPrefix + m_sCachePrefix;
                }

                if (!path_separator(m_sCachePrefix.back())) {
                    m_sCachePrefix += '/';
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetConfFile(LPCTSTR AValue) {
            if (m_sConfFile != AValue) {
                m_sConfFile = AValue;
                if (!path_separator(m_sConfFile.front())) {
                    m_sConfFile = m_sPrefix + m_sConfFile;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetConfParam(LPCTSTR AValue) {
            if (m_sConfParam != AValue) {
                m_sConfParam = AValue;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetSignal(LPCTSTR AValue) {
            if (m_sSignal != AValue) {
                m_sSignal = AValue;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetPidFile(LPCTSTR AValue) {
            if (m_sPidFile != AValue) {
                m_sPidFile = AValue;
                if (!path_separator(m_sPidFile.front())) {
                    m_sPidFile = m_sPrefix + m_sPidFile;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetLockFile(LPCTSTR AValue) {
            if (m_sLockFile != AValue) {
                m_sLockFile = AValue;
                if (!path_separator(m_sLockFile.front())) {
                    m_sLockFile = m_sPrefix + m_sLockFile;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetErrorLog(LPCTSTR AValue) {
            if (m_sErrorLog != AValue) {
                m_sErrorLog = AValue;
                if (!path_separator(m_sErrorLog.front())) {
                    m_sErrorLog = m_sPrefix + m_sErrorLog;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetAccessLog(LPCTSTR AValue) {
            if (m_sAccessLog != AValue) {
                m_sAccessLog = AValue;
                if (!path_separator(m_sAccessLog.front())) {
                    m_sAccessLog = m_sPrefix + m_sAccessLog;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetPostgresLog(LPCTSTR AValue) {
            if (m_sPostgresLog != AValue) {
                m_sPostgresLog = AValue;
                if (!path_separator(m_sPostgresLog.front())) {
                    m_sPostgresLog = m_sPrefix + m_sPostgresLog;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetDocRoot(LPCTSTR AValue) {
            if (m_sDocRoot != AValue) {
                m_sDocRoot = AValue;
                if (!path_separator(m_sDocRoot.front())) {
                    m_sDocRoot = m_sPrefix + m_sDocRoot;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetLocale(LPCTSTR AValue) {
            if (m_sLocale != AValue) {
                m_sLocale = AValue;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::SetDefault() {
            m_uErrorCount = 0;

            m_nWorkers = 1;
            m_nPort = 4977;

            m_nTimeOut = 5000;
            m_nConnectTimeOut = 5;

            m_fMaster = true;
            m_fDaemon = true;

            m_fPostgresConnect = false;
            m_fPostgresNotice = false;

            m_nPostgresPollMin = 5;
            m_nPostgresPollMax = 10;

            SetUser(m_sUser.empty() ? APP_DEFAULT_USER : m_sUser.c_str());
            SetGroup(m_sGroup.empty() ? APP_DEFAULT_GROUP : m_sGroup.c_str());

            SetListen(m_sListen.empty() ? APP_DEFAULT_LISTEN : m_sListen.c_str());

            SetPrefix(m_sPrefix.empty() ? APP_PREFIX : m_sPrefix.c_str());
            SetConfPrefix(m_sConfPrefix.empty() ? APP_CONF_PREFIX : m_sConfPrefix.c_str());
            SetCachePrefix(m_sCachePrefix.empty() ? APP_CACHE_PREFIX : m_sCachePrefix.c_str());
            SetConfFile(m_sConfFile.empty() ? APP_CONF_FILE : m_sConfFile.c_str());
            SetDocRoot(m_sDocRoot.empty() ? APP_DOC_ROOT : m_sDocRoot.c_str());

            SetLocale(m_sLocale.empty() ? APP_DEFAULT_LOCALE : m_sLocale.c_str());

            SetPidFile(m_sPidFile.empty() ? APP_PID_FILE : m_sPidFile.c_str());
            SetLockFile(m_sLockFile.empty() ? APP_LOCK_FILE : m_sLockFile.c_str());

            SetErrorLog(m_sErrorLog.empty() ? APP_ERROR_LOG_FILE : m_sErrorLog.c_str());
            SetAccessLog(m_sAccessLog.empty() ? APP_ACCESS_LOG_FILE : m_sAccessLog.c_str());
            SetPostgresLog(m_sPostgresLog.empty() ? APP_POSTGRES_LOG_FILE : m_sPostgresLog.c_str());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::DefaultCommands() {

            Clear();

            Add(new CConfigCommand(_T("main"), _T("user"), m_sUser.c_str(), std::bind(&CConfig::SetUser, this, _1)));
            Add(new CConfigCommand(_T("main"), _T("group"), m_sGroup.c_str(), std::bind(&CConfig::SetGroup, this, _1)));

            Add(new CConfigCommand(_T("main"), _T("workers"), &m_nWorkers));
            Add(new CConfigCommand(_T("main"), _T("master"), &m_fMaster));
            Add(new CConfigCommand(_T("main"), _T("locale"), m_sLocale.c_str(), std::bind(&CConfig::SetLocale, this, _1)));

            Add(new CConfigCommand(_T("daemon"), _T("daemon"), &m_fDaemon));
            Add(new CConfigCommand(_T("daemon"), _T("pid"), m_sPidFile.c_str(), std::bind(&CConfig::SetPidFile, this, _1)));

            Add(new CConfigCommand(_T("server"), _T("listen"), m_sListen.c_str(), std::bind(&CConfig::SetListen, this, _1)));
            Add(new CConfigCommand(_T("server"), _T("port"), &m_nPort));
            Add(new CConfigCommand(_T("server"), _T("timeout"), &m_nTimeOut));
            Add(new CConfigCommand(_T("server"), _T("root"), m_sDocRoot.c_str(), std::bind(&CConfig::SetDocRoot, this, _1)));

            Add(new CConfigCommand(_T("cache"), _T("prefix"), m_sCachePrefix.c_str(), std::bind(&CConfig::SetCachePrefix, this, _1)));

            Add(new CConfigCommand(_T("log"), _T("error"), m_sErrorLog.c_str(), std::bind(&CConfig::SetErrorLog, this, _1)));
            Add(new CConfigCommand(_T("server"), _T("log"), m_sAccessLog.c_str(), std::bind(&CConfig::SetAccessLog, this, _1)));
            Add(new CConfigCommand(_T("postgres"), _T("log"), m_sPostgresLog.c_str(), std::bind(&CConfig::SetPostgresLog, this, _1)));

            Add(new CConfigCommand(_T("postgres"), _T("connect"), &m_fPostgresConnect));
            Add(new CConfigCommand(_T("postgres"), _T("notice"), &m_fPostgresNotice));
            Add(new CConfigCommand(_T("postgres"), _T("timeout"), &m_nConnectTimeOut));

            Add(new CConfigCommand(_T("postgres/poll"), _T("min"), &m_nPostgresPollMin));
            Add(new CConfigCommand(_T("postgres/poll"), _T("max"), &m_nPostgresPollMax));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::LoadLogFilesDefault() {
            m_LogFiles.AddPair(_T("error"), m_sErrorLog.c_str());
        };
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::Parse() {

            if (GetCount() == 0)
                throw Delphi::Exception::Exception(_T("Command list is empty"));

            CConfigCommand *C;
            CVariant V;

            if (FileExists(m_sConfFile.c_str())) {

                CIniFile F(m_sConfFile.c_str());
                F.OnIniFileParseError(std::bind(&CConfig::OnIniFileParseError, this, _1, _2, _3, _4, _5, _6));

                for (int i = 0; i < Count(); ++i) {
                    C = Commands(i);

                    switch (C->Type()) {
                        case ctInteger:
                            V = F.ReadInteger(C->Section(), C->Ident(), C->Default().vasInteger);
                            break;

                        case ctDouble:
                            V = F.ReadFloat(C->Section(), C->Ident(), C->Default().vasDouble);
                            break;

                        case ctBoolean:
                            V = F.ReadBool(C->Section(), C->Ident(), C->Default().vasBoolean);
                            break;

                        case ctDateTime:
                            V = F.ReadDateTime(C->Section(), C->Ident(), C->Default().vasDouble);
                            break;

                        default:
                            V = new CString();
                            F.ReadString(C->Section(), C->Ident(), C->Default().vasStr, *V.vasCString);
                            break;
                    }

                    C->Value(V);
                }

                m_LogFiles.Clear();
                F.ReadSectionValues(_T("log"), &m_LogFiles);
                if ((m_LogFiles.Count() == 0) || !CheckLogFiles())
                    LoadLogFilesDefault();

                m_PostgresConnInfo.Clear();
                F.ReadSectionValues(_T("postgres/conninfo"), &m_PostgresConnInfo);

                if (m_PostgresConnInfo.Count() == 0) {
                    m_fPostgresConnect = false;
                }
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::Reload() {
            SetDefault();
            DefaultCommands();
            Parse();
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CConfig::CheckLogFiles() {
            u_int Level;

            for (int I = 0; I < m_LogFiles.Count(); ++I) {

                const CString &Key = m_LogFiles.Names(I);

                Level = GetLogLevelByName(Key.c_str());
                if (Level == 0) {
                    Log()->Error(APP_LOG_EMERG, 0, ConfMsgInvalidKey _T(" - ignored and set by default"), _T("log"),
                                 Key.c_str(), m_sConfFile.c_str());
                    return false;
                }

                const CString &Value = m_LogFiles.Values(Key);
                if (!path_separator(m_LogFiles.Values(Key).front())) {
                    m_LogFiles[I] = Key + _T("=") + m_sPrefix + Value;
                }
            }

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CConfig::OnIniFileParseError(Pointer Sender, LPCTSTR lpszSectionName, LPCTSTR lpszKeyName,
                LPCTSTR lpszValue, LPCTSTR lpszDefault, int Line) {

            m_uErrorCount++;
            if ((lpszValue == nullptr) || (lpszValue[0] == '\0')) {
                if (Flags().test_config || (lpszDefault == nullptr) || (lpszDefault[0] == '\0'))
                    Log()->Error(APP_LOG_EMERG, 0, ConfMsgEmpty, lpszSectionName, lpszKeyName, m_sConfFile.c_str(), Line);
                else
                    Log()->Error(APP_LOG_EMERG, 0, ConfMsgEmpty _T(" - ignored and set by default: \"%s\""), lpszSectionName,
                                lpszKeyName, m_sConfFile.c_str(), Line, lpszDefault);
            } else {
                if (Flags().test_config || (lpszDefault == nullptr) || (lpszDefault[0] == '\0'))
                    Log()->Error(APP_LOG_EMERG, 0, ConfMsgInvalidValue, lpszSectionName, lpszKeyName, lpszValue,
                                m_sConfFile.c_str(), Line);
                else
                    Log()->Error(APP_LOG_EMERG, 0, ConfMsgInvalidValue _T(" - ignored and set by default: \"%s\""), lpszSectionName, lpszKeyName, lpszValue,
                                m_sConfFile.c_str(), Line, lpszDefault);
            }
        }

    }
}
};