/*++

Program name:

  ocpp

Module Name:

  ocpp.hpp

Notices:

  OCPP Central System Service

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_OCPP_HPP
#define APOSTOL_OCPP_HPP

#include "../../version.h"
//----------------------------------------------------------------------------------------------------------------------

#define APP_VERSION      AUTO_VERSION
#define APP_VER          APP_NAME "/" APP_VERSION
//----------------------------------------------------------------------------------------------------------------------

#include "Header.hpp"
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace OCPP {

        class CCentralSystem: public CApplication {
        protected:

            void ParseCmdLine() override;
            void ShowVersionInfo() override;

            void StartProcess() override;

        public:

            CCentralSystem(int argc, char *const *argv): CApplication(argc, argv) {

            };

            ~CCentralSystem() override = default;

            static class CCentralSystem *Create(int argc, char *const *argv) {
                return new CCentralSystem(argc, argv);
            };

            inline void Destroy() override { delete this; };

            void Run() override;

        };
    }
}

using namespace Apostol::OCPP;
}

#endif //APOSTOL_OCPP_HPP
