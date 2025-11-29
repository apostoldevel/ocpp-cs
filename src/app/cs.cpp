/*++

Program name:

  Central System

Module Name:

  cs.cpp

Notices:

  OCPP Central System Service

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#include "cs.hpp"
//----------------------------------------------------------------------------------------------------------------------

#define exit_failure(msg) {                                 \
  if (GLog != nullptr)                                      \
    GLog->Error(APP_LOG_EMERG, 0, msg);                         \
  else                                                      \
    std::cerr << APP_NAME << ": " << (msg) << std::endl;    \
  exitcode = EXIT_FAILURE;                                  \
}
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace OCPP {

        void CCentralSystem::ShowVersionInfo() {

            std::cerr << APP_NAME " version: " APP_VERSION " (" APP_DESCRIPTION ")" LINEFEED << std::endl;

            if (Config()->Flags().show_help) {
                std::cerr << "Usage: " APP_NAME << " [-?hvVt] [-s signal] [-c filename]"
                             " [-p prefix] [-g directives] [-w workers] [-l locale]" LINEFEED
                             LINEFEED
                             "Options:" LINEFEED
                             "  -?,-h         : this help" LINEFEED
                             "  -v            : show version and exit" LINEFEED
                             "  -V            : show version and configure options then exit" LINEFEED
                             "  -t            : test configuration and exit" LINEFEED
                             "  -s signal     : send signal to a master process: stop, quit, reopen, reload" LINEFEED
                             "  -c filename   : set configuration file (default: " APP_CONF_FILE ")" LINEFEED
                             #ifdef APP_PREFIX
                             "  -p prefix     : set prefix path (default: " APP_PREFIX ")" LINEFEED
                             #else
                             "  -p prefix     : set prefix path (default: NONE)" LINEFEED
                             #endif
                             "  -g directives : set global directives out of configuration file" LINEFEED
                             "  -w workers    : set count of worker processes (default: 0 - automatically)" LINEFEED
                             "  -l locale     : set locale (default: " APP_DEFAULT_LOCALE ")" LINEFEED
                          << std::endl;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCentralSystem::ParseCmdLine() {

            LPCTSTR P;

            for (int i = 1; i < argc(); ++i) {

                P = argv()[i].c_str();

                if (*P++ != '-') {
                    throw Delphi::Exception::ExceptionFrm(_T("invalid option: \"%s\""), P);
                }

                while (*P) {

                    switch (*P++) {

                        case '?':
                        case 'h':
                            Config()->Flags().show_version = true;
                            Config()->Flags().show_help = true;
                            break;

                        case 'v':
                            Config()->Flags().show_version = true;
                            break;

                        case 'V':
                            Config()->Flags().show_version = true;
                            Config()->Flags().show_configure = true;
                            break;

                        case 't':
                            Config()->Flags().test_config = true;
                            break;

                        case 'p':
                            if (*P) {
                                Config()->Prefix(P);
                                goto next;
                            }

                            if (!argv()[++i].empty()) {
                                Config()->Prefix(argv()[i]);
                                goto next;
                            }

                            throw Delphi::Exception::Exception(_T("option \"-p\" requires directory name"));

                        case 'c':
                            if (*P) {
                                Config()->ConfFile(P);
                                goto next;
                            }

                            if (!argv()[++i].empty()) {
                                Config()->ConfFile(argv()[i]);
                                goto next;
                            }

                            throw Delphi::Exception::Exception(_T("option \"-c\" requires file name"));

                        case 'g':
                            if (*P) {
                                Config()->ConfParam(P);
                                goto next;
                            }

                            if (!argv()[++i].empty()) {
                                Config()->ConfParam(argv()[i]);
                                goto next;
                            }

                            throw Delphi::Exception::Exception(_T("option \"-g\" requires parameter"));

                        case 'w':
                            if (*P) {
                                Config()->Workers(StrToIntDef(P, 0));
                                goto next;
                            }

                            if (++i < argc() && !argv()[i].empty()) {
                                Config()->Workers(StrToIntDef(argv()[i].c_str(), 0));
                                goto next;
                            }

                            throw Delphi::Exception::Exception(_T("option \"-w\" requires parameter"));

                        case 's':
                            if (*P) {
                                Config()->Signal(P);
                            } else if (!argv()[++i].empty()) {
                                Config()->Signal(argv()[i]);
                            } else {
                                throw Delphi::Exception::Exception(_T("option \"-s\" requires parameter"));
                            }

                            if (   Config()->Signal() == "stop"
                                   || Config()->Signal() == "quit"
                                   || Config()->Signal() == "reopen"
                                   || Config()->Signal() == "reload")
                            {
                                ProcessType(ptSignaller);
                                goto next;
                            }

                            throw Delphi::Exception::ExceptionFrm(_T("invalid option: \"-s %s\""), Config()->Signal().c_str());

                        case 'l':
                            if (*P) {
                                Config()->Locale(P);
                                goto next;
                            }

                            if (!argv()[++i].empty()) {
                                Config()->Locale(argv()[i]);
                                goto next;
                            }

                            throw Delphi::Exception::Exception(_T("option \"-l\" requires locale"));

                        default:
                            throw Delphi::Exception::ExceptionFrm(_T("invalid option: \"%c\""), *(P - 1));
                    }
                }

                next:

                continue;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCentralSystem::CreateCustomProcesses() {
            if (Config()->Helper()) {
                m_ProcessType = ptHelper;
            }

            if (Config()->IniFile().ReadBool("process/ChargePoint", "enable", false)) {
                Application()->SignalProcess(AddProcess<CCPEmulator>());
                m_ProcessType = ptCustom;
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCentralSystem::StartProcess() {
            CApplication::StartProcess();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CCentralSystem::Run() {
            CApplication::Run();
        }
        //--------------------------------------------------------------------------------------------------------------
    }
}
}
//----------------------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {

    int exitcode;

    DefaultLocale.SetLocale(APP_DEFAULT_LOCALE);
    
    CCentralSystem cs(argc, argv);

    try
    {
        cs.Name() = APP_NAME;
        cs.Description() = APP_DESCRIPTION;
        cs.Version() = APP_VERSION;
        cs.Title() = APP_VER;

        cs.Run();

        exitcode = cs.ExitCode();
    }
    catch (std::exception& e)
    {
        exit_failure(e.what());
    }
    catch (...)
    {
        exit_failure("Unknown error...");
    }

    return exitcode;
}
//----------------------------------------------------------------------------------------------------------------------
