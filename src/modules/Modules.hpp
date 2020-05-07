/*++

Program name:

  apostol

Module Name:

  Modules.hpp

Notices:

  Apostol Web Service

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_MODULES_HPP
#define APOSTOL_MODULES_HPP
//----------------------------------------------------------------------------------------------------------------------

#include "Module.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include "CentralSystem/Service.hpp"
//----------------------------------------------------------------------------------------------------------------------

static void CreateModules(CModuleManager *AManager) {
    CCSService::CreateModule(AManager);
}

#endif //APOSTOL_MODULES_HPP
