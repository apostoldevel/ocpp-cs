/*++

Program name:

  apostol

Module Name:

  Workers.hpp

Notices:

  Apostol Web Service

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_WORKERS_HPP
#define APOSTOL_WORKERS_HPP
//----------------------------------------------------------------------------------------------------------------------

#include "CentralSystem/OCPP.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include "CentralSystem/Service.hpp"
//----------------------------------------------------------------------------------------------------------------------

static inline void CreateWorkers(CModuleProcess *AProcess) {
    CCSService::CreateModule(AProcess);
}

#endif //APOSTOL_WORKERS_HPP
