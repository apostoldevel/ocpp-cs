![image](https://user-images.githubusercontent.com/91010417/230783150-ea57f6c4-ba8a-43cf-a033-ef5ffa3a19ff.png)

# OCPP Central System

**OCPP Central System** and **Charge Point emulator**, implementation in C++.

The software stack consists of a compilation of source code, libraries and scripts.

Built on base [Apostol](https://github.com/ufocomp/apostol).

Overview
-
Open Charge Point Protocol [OCPP](http://ocppforum.net) is a communication protocol between multiple charging stations ("charge points") and a single management software ("central system").

The **OCPP Central System** supports all commands for released versions of the OCPP protocol (1.5 and 1.6). 

Version 1.5 uses SOAP over HTTP as the RPC/transport protocol. Version 1.6 uses SOAP and JSON over WebSocket protocol.

See [Releases](https://github.com/apostoldevel/ocpp-cs/releases) for more details.

API
-
We use OpenAPI to interact with the Central System (CS). You can directly open Swagger UI through [http://cs.ocpp-css.com/docs](http://cs.ocpp-css.com/docs).

Alternatively, you can use any OpenAPI client to import the [api.yaml](https://github.com/apostoldevel/ocpp-cs/blob/master/www/docs/api.yaml) file from our repository (download).

Authorize:
~~~
username: demo
password: demo
~~~

RFID-card:
~~~
idTag: demo
~~~

Framework
-
The OCPP Central System is a set of C++ libraries for building OCPP applications. The toolkit consists of several libraries, most of which depend on the foundational [libdelphi](https://github.com/ufocomp/libdelphi) library.

This code can be used as a framework for creating your own **Central System** or preparing firmware for a **Charging Station**.

Docker
-

### Docker Hub

You can get image of the central system from the docker hub:

~~~
docker pull apostoldevel/cs
~~~

How to use this image:
~~~
docker run -d -p 9220:9220 --rm --name cs apostoldevel/cs
~~~

Then you can hit http://localhost:9220 or http://host-ip:9220 in your browser.

Swagger will also be available at http://localhost:9220/docs/ or http://host-ip:9220/docs/ in your browser.

Building for a container does not require authorization.

### Custom build

You can build an image yourself:

~~~
git clone https://github.com/apostoldevel/ocpp-cs.git
~~~

Edit file `ocpp-cs/docker/www/config.js` according to your requirements. Specify the correct addresses of your server.

Edit file `ocpp-cs/docker/conf/sites/default.json` add IP address your server:

For example, your server IP address is `192.168.1.100` or DNS name is `ocpp-server`.

~~~
"hosts": ["localhost:9220", "192.168.1.100:9220", "ocpp-server:9220"]
~~~

Build `cs` image:
###### If you already had a container named `cs`, delete it.
~~~
cd ocpp-cs/docker
docker build -t cs .
~~~

Run docker:
~~~
docker run -d -p 9220:9220 --name cs cs
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
username: demo
password: demo
~~~

RFID-card:
~~~
idTag: demo
~~~

Attention
-
The production version is designed to work with a database and all business logic is implemented in PL/pgSQL (the code is not included in this assembly).

To build in emulator mode, change the following settings in the [CMakeLists.txt](https://github.com/apostoldevel/ocpp-cs/blob/master/CMakeLists.txt) file:
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

1. Download [OCPP Central System](https://github.com/ufocomp/ocpp-cs/archive/master.zip);
1. Unpack;
1. Download [libdelphi](https://github.com/ufocomp/libdelphi/archive/master.zip);
1. Unpack in `src/lib/delphi`;
1. Configure `CMakeLists.txt` (of necessity);
1. Build and compile (see below).

To install (with Git) you need:
~~~
git clone https://github.com/apostoldevel/ocpp-cs.git
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
cd ocpp-cs
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
###### If `INSTALL_AS_ROOT` set to `ON`.

`cs` - it is a Linux system service (daemon).

To manage `cs` use standard service management commands.

To start, run:
~~~
sudo systemctl start cs
~~~

To check the status, run:
~~~
sudo systemctl status cs
~~~
