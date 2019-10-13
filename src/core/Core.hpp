/*++

Library name:

  apostol-core

Module Name:

  Core.hpp

Notices:

  Apostol Core

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

#ifndef APOSTOL_CORE_HPP
#define APOSTOL_CORE_HPP

#include <iostream>
#include <cstring>
#include <cstdarg>
#include <climits>
#include <ctime>
#include <functional>
#include <unistd.h>
#include <fcntl.h>
//#include <sys/time.h>
//#include <signal.h>
#include <execinfo.h>
#include <wait.h>
#include <pwd.h>
#include <grp.h>
//----------------------------------------------------------------------------------------------------------------------

//using namespace std;
//----------------------------------------------------------------------------------------------------------------------

#include "delphi.hpp"
//----------------------------------------------------------------------------------------------------------------------

typedef intptr_t        int_t;
typedef uintptr_t       uint_t;
//----------------------------------------------------------------------------------------------------------------------

#ifndef LF
#define LF '\n'
#endif

#define CRLF   "\r\n"
//----------------------------------------------------------------------------------------------------------------------

#define linefeed(p)          *(p)++ = LF;
#define LINEFEED_SIZE        1
#define LINEFEED             "\x0a"
//----------------------------------------------------------------------------------------------------------------------

#define path_separator(c)    ((c) == '/')
//----------------------------------------------------------------------------------------------------------------------

#include "Log.hpp"
#include "Config.hpp"
//----------------------------------------------------------------------------------------------------------------------

class CGlobalComponent: public CLogComponent, public CConfigComponent {
public:

    CGlobalComponent(): CLogComponent(), CConfigComponent() {
    };

    static CConfig *Config() { return GConfig; };

    static CLog *Log(){ return GLog; };

};
//----------------------------------------------------------------------------------------------------------------------

#include "Module.hpp"
#include "Process.hpp"
#include "Application.hpp"
//----------------------------------------------------------------------------------------------------------------------

#endif //APOSTOL_CORE_HPP
