# OCPP Central System Service

OCPP (Open Charge Point Protocol) implementation in C++. Built on base [Apostol](https://github.com/ufocomp/apostol).

The software stack consists of a compilation of source code, libraries and scripts.

Overview
-
Open Charge Point Protocol [OCPP](http://ocppforum.net) is a communication protocol between multiple charging stations ("charge points") and a single management software ("central system").

Currently, two versions of OCPP (1.5 and 1.6) are released. A draft is being prepared for the new version (2.0). Version 1.5 uses SOAP over HTTP as the RPC/transport protocol. Version 1.6 uses SOAP and JSON over WebSocket protocol.

Build and installation
-
Build required:

1. Compiler C++;
1. [CMake](https://cmake.org);
1. Library [libdelphi](https://github.com/ufocomp/libdelphi/) (Delphi classes for C++);
1. Library [libpq-dev](https://www.postgresql.org/download/) (libraries and headers for C language frontend development);
1. Library [postgresql-server-dev-10](https://www.postgresql.org/download/) (libraries and headers for C language backend development).

###### **ATTENTION**: You do not need to install [libdelphi](https://github.com/ufocomp/libdelphi/), just download and put it in the `src/lib` directory of the project.

To install the C ++ compiler and necessary libraries in Ubuntu, run:
~~~
sudo apt-get install build-essential libssl-dev libcurl4-openssl-dev make cmake gcc g++
~~~

To install PostgreSQL, use the instructions for [this](https://www.postgresql.org/download/) link.

###### A detailed description of the installation of C ++, CMake, IDE, and other components necessary for building the project is not included in this guide. 

To install (without Git) you need:

1. Download [OCPP Central System Service](https://github.com/ufocomp/apostol-ocpp/archive/master.zip);
1. Unpack;
1. Download [libdelphi](https://github.com/ufocomp/libdelphi/archive/master.zip);
1. Unpack in `src/lib/delphi`;
1. Configure `CMakeLists.txt` (of necessity);
1. Build and compile (see below).

To install (with Git) you need:
~~~
git clone https://github.com/ufocomp/apostol-ocpp.git
~~~

###### Build:
~~~
cd apostol-ocpp
./configure
~~~

###### Compilation and installation:
~~~
cd cmake-build-release
make
sudo make install
~~~

By default, **ocpp** will be set to:
~~~
/usr/sbin
~~~

The configuration file and the necessary files for operation, depending on the installation option, will be located in:
~~~
/etc/apostol-ocpp
or
~/apostol-ocpp
~~~

Run
-
###### If **`INSTALL_AS_ROOT`** set to `ON`.

**`ocpp`** - it is a Linux system service (daemon). 

To manage **`ocpp`** use standard service management commands.

To start, run:
~~~
sudo service ocpp start
~~~

To check the status, run:
~~~
sudo service ocpp status
~~~
