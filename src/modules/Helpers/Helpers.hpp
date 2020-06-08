/*++

Program name:

  apostol

Module Name:

  Helpers.hpp

Notices:

  Apostol Web Service

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_HELPERS_HPP
#define APOSTOL_HELPERS_HPP
//----------------------------------------------------------------------------------------------------------------------

#include "CertificateDownloader/CertificateDownloader.hpp"
//----------------------------------------------------------------------------------------------------------------------

static inline void CreateHelpers(CModuleProcess *AProcess) {
    CCertificateDownloader::CreateModule(AProcess);
}

#endif //APOSTOL_HELPERS_HPP
