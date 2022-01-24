# OCPP Central System

**OCPP Central System** and **Charge Point emulator**, implementation in C++.

The software stack consists of a compilation of source code, libraries and scripts.

Built on base [Apostol](https://github.com/ufocomp/apostol).

Overview
-
Open Charge Point Protocol [OCPP](http://ocppforum.net) is a communication protocol between multiple charging stations ("charge points") and a single management software ("central system").

The **OCPP Central System** supports all commands for released versions of the OCPP protocol (1.5 and 1.6). 

Version 1.5 uses SOAP over HTTP as the RPC/transport protocol. Version 1.6 uses SOAP and JSON over WebSocket protocol.

See [Releases](https://github.com/apostoldevel/apostol-cs/releases) for more details.

Framework
-
The OCPP Central System is a set of C++ libraries for building OCPP applications. The toolkit consists of several libraries, most of which depend on the foundational [libdelphi](https://github.com/ufocomp/libdelphi) library.

This code can be used as a framework for creating your own **Central System** or preparing firmware for a **Charging Station**.

Swagger
-
We use OpenAPI to interact with the full node. You can directly open Swagger UI through [https://ocpp-css.ru/docs](https://ocpp-css.ru/docs).

Alternatively, you can use any OpenAPI client to import the [api.yaml](https://ocpp-css.ru/docs/api.yaml) file from our repository (download).

Authorize:
~~~
login: demo
password: demo
~~~

Demonstration
-
You can connect your station to a demo central system.

---
Connection addresses:
- WebSocket: ws://ws.ocpp-css.com/ocpp
- SOAP: http://soap.ocpp-css.com/Ocpp
---

To control the charging station, use the web shell at:
- [http://cs.ocpp-css.com](http://cs.ocpp-css.com)

Authorize:
~~~
login: demo
password: demo
~~~

RFID-card:
~~~
idTag: demo
~~~

Attention
-
The production version is designed to work with a database and all business logic is implemented in PL/pgSQL (the code is not included in this assembly).

To build in emulator mode, change the following settings in the [CMakeLists.txt](https://github.com/apostoldevel/apostol-cs/blob/master/CMakeLists.txt) file:
~~~
WITH_AUTHORIZATION OFF
WITH_POSTGRESQL OFF
~~~

Build and installation
-
Build required:

1. Compiler C++;
1. [CMake](https://cmake.org);
1. Library [libdelphi](https://github.com/ufocomp/libdelphi/) (Delphi classes for C++);
1. Library [libpq-dev](https://www.postgresql.org/download/) (libraries and headers for C language frontend development);
1. Library [postgresql-server-dev-all](https://www.postgresql.org/download/) (libraries and headers for C language backend development).

###### **ATTENTION**: You do not need to install [libdelphi](https://github.com/ufocomp/libdelphi/), just download and put it in the `src/lib` directory of the project.

To install the C ++ compiler and necessary libraries in Ubuntu, run:
~~~
sudo apt-get install build-essential libssl-dev libcurl4-openssl-dev make cmake gcc g++
~~~

To install PostgreSQL, use the instructions for [this](https://www.postgresql.org/download/) link.

###### A detailed description of the installation of C ++, CMake, IDE, and other components necessary for building the project is not included in this guide.

To install (without Git) you need:

1. Download [OCPP Central System](https://github.com/ufocomp/apostol-cs/archive/master.zip);
1. Unpack;
1. Download [libdelphi](https://github.com/ufocomp/libdelphi/archive/master.zip);
1. Unpack in `src/lib/delphi`;
1. Configure `CMakeLists.txt` (of necessity);
1. Build and compile (see below).

To install (with Git) you need:
~~~
git clone https://github.com/apostoldevel/apostol-cs.git
~~~

###### CMake configuration:
~~~
/// Install as root. 
/// Disable for local installation.
/// Default: ON 
INSTALL_AS_ROOT = {ON | OFF}

/// Build with authorization OAuth 2.0 for production release. 
/// Disable for emulator mode. 
/// Default: ON
WITH_AUTHORIZATION = {ON | OFF}

/// Build with PostgreSQL for production release. 
/// Disable for emulator mode. 
/// Default: ON
WITH_POSTGRESQL = {ON | OFF}
~~~

###### Build:
~~~
cd apostol-cs
./configure
~~~

###### Compilation and installation:
~~~
cd cmake-build-release
make
sudo make install
~~~

By default, **cs** will be set to:
~~~
/usr/sbin
~~~

The configuration file and the necessary files for operation, depending on the installation option, will be located in:
~~~
/etc/cs
or
~/cs
~~~

Run
-
###### If **`INSTALL_AS_ROOT`** set to `ON`.

**`cs`** - it is a Linux system service (daemon).

To manage **`cs`** use standard service management commands.

To start, run:
~~~
sudo service cs start
~~~

To check the status, run:
~~~
sudo service cs status
~~~

Donate
-
---
~~~
BTC: 1MvvFNE89NK1zQguy9RhXH3PGmSf7RcahB
ETH: 0x417bed7f4b156d952503fb8f91644194b48dded2
~~~
Any help is priceless, thanks.

---
