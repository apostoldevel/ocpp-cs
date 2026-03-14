[![ru](https://img.shields.io/badge/lang-ru-green.svg)](README.ru-RU.md)

![image](https://user-images.githubusercontent.com/91010417/230783150-ea57f6c4-ba8a-43cf-a033-ef5ffa3a19ff.png)

# OCPP Central System

**OCPP Central System** — Central system and charge station emulator in C++20.

Built on [A-POST-OL](https://github.com/apostoldevel/libapostol) (libapostol) — a high-performance C++20 framework with a single `epoll` event loop for HTTP, WebSocket, and PostgreSQL.

## Overview

**OCPP Central System** is both a ready-to-use solution that you can easily integrate into your project and a set of tools for developing applications that work with the OCPP protocol.

This solution can be used for:
- Developing or integrating a central charging station system;
- Emulating charging stations;
- Developing charging station firmware.

## OCPP

Open Charge Point Protocol [OCPP](http://ocppforum.net) is a communication protocol between charging stations ("charge points") and a central management system ("central system").

**OCPP Central System** supports all commands for OCPP protocol versions 1.5 and 1.6.

Version 1.5 uses SOAP over HTTP as the RPC/transport protocol. Version 1.6 uses SOAP and JSON over WebSocket.

## API

We use OpenAPI for interaction with the Central System. You can directly access Swagger UI at [http://cs.ocpp-css.com/docs](http://cs.ocpp-css.com/docs).

Additionally, you can use any OpenAPI client to import the [api.yaml](https://github.com/apostoldevel/ocpp-cs/blob/master/www/docs/api.yaml) file from our repository.

## Demonstration

You can connect your charging station to the demo version of the Central System.

---
Connection addresses:
- WebSocket: ws://ws.ocpp-css.com/ocpp
- SOAP: http://soap.ocpp-css.com/Ocpp
---

To manage the charging station, use the web interface at:
- [http://cs.ocpp-css.com](http://cs.ocpp-css.com)

Authorization:
```
username: demo
password: demo
```

RFID card:
```
idTag: demo
```

## Build & Install

The easiest way to install the Central System is as a container.

### Docker Hub

```shell
docker pull apostoldevel/cs
```

Run the container:
```shell
docker run -p 9220:9220 --network host --env WEBHOOK_URL=https://api.ocpp-css.com/api/v1/ocpp/parse --rm --name cs apostoldevel/cs
```

### Building the Container Image

Clone the repository:
```shell
git clone https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs
```

Configure it according to your requirements:

- Edit the `./docker/conf/default.json` file, paying special attention to the `"webhook"` section;
- Edit the `./docker/www/config.js` file to specify your server's domain name or IP address;
- Edit the `./docker/conf/sites/default.json` file to add your server's IP address:

  For example, your server's IP address is `192.168.1.100` or DNS name `cs.example.com`.
  ```json
  {
    "hosts": ["cs.example.com", "cs.example.com:9220", "192.168.1.100:9220", "localhost:9220"]
  }
  ```

Create and start the container with a single command:
```shell
docker compose up
```

### Web Application

After starting the container, the Central System will be available at http://localhost:9220 in your browser.

Swagger UI will also be available at http://localhost:9220/docs/.

###### Launching from the container does not require authorization.

### Building from Source Code

#### Prerequisites

- **C++20** compiler: GCC 12+ or Clang 16+
- **CMake** 3.25+
- `libssl-dev`, `libpq-dev`
- **PostgreSQL** 12+ (optional — can be disabled)

To install the C++ compiler and necessary libraries on Debian/Ubuntu:
```shell
sudo apt-get install build-essential libssl-dev libcurl4-openssl-dev make cmake gcc g++
```

Clone the repository:
```shell
git clone https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs
```

#### Build

```shell
# Configure (runs cmake)
./configure               # release build
./configure --debug       # debug build

# Build
cmake --build cmake-build-release --parallel $(nproc)

# Install
sudo cmake --install cmake-build-release
```

By default, **cs** will be installed in `/usr/sbin/`. Configuration files in `/etc/cs/`.

#### Local Development

```shell
mkdir -p logs
./cmake-build-debug/cs -p . -c conf/default.json
```

#### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `INSTALL_AS_ROOT` | ON | Install as root. Disable for local installation. |
| `WITH_POSTGRESQL` | ON | PostgreSQL support for production integration. Disable for standalone emulator. |
| `WITH_SSL` | ON | TLS, JWT verification, OAuth 2.0 authorization. |

To build without database integration:
```cmake
WITH_POSTGRESQL OFF
WITH_SSL OFF
```

## Integration

There are several ways to integrate the Central System with your project.

### Webhook

The simplest way is through a Webhook endpoint.

In the Central System configuration file `conf/default.json`, there is a `"webhook"` section:

```json
{
  "webhook": {
    "enable": false,
    "url": "http://localhost:8080/api/v1/ocpp/parse",
    "authorization": "Basic",
    "username": "ocpp",
    "password": "ocpp",
    "token": ""
  }
}
```

In this section, you can specify the endpoint URL to which the Central System will forward packets received from charging stations.

Specifically, these are ten commands from section **4. Operations Initiated by Charge Point** of the OCPP v1.6 specification:

- 4.1. Authorize
- 4.2. Boot Notification
- 4.3. Data Transfer
- 4.4. Diagnostics Status Notification
- 4.5. Firmware Status Notification
- 4.6. Heartbeat
- 4.7. Meter Values
- 4.8. Start Transaction
- 4.9. Status Notification
- 4.10. Stop Transaction

Additionally, you can set up authorization parameters on your server side, which will receive requests from the Central System.

Data from the Central System will be in the following JSON format:
```json
{
  "identity": "string",
  "uniqueId": "string",
  "action": "string",
  "payload": "JSON Object",
  "account": "string"
}
```
Where:
- `identity`: Required. The charging station identifier;
- `uniqueId`: Required. The data packet (request) identifier;
- `action`: Required. The action name;
- `payload`: Required. Payload — data from the charging station;
- `account`: Optional. The user account identifier in your system.

###### Using `account`, you can associate the charging station with a user account in your system if the project's business logic requires it. Typically, the charging station's connection address to the central system is specified in the format `ws://host/ocpp/EM-A0000001`. If you append an additional value to the charging station identifier `EM-A0000001`, for example: `/EM-A0000001/AC0001`, then `AC0001` will be the user account identifier.

The Central System will expect a response from your system in the same JSON format. Field values (`identity`, `uniqueId`, `action`) should be filled with values from the incoming request, but the `payload` should contain response data to the `action` in the OCPP protocol specification format.

Example request:
```json
{
  "identity": "EM-A0000001",
  "uniqueId": "25cf07c9ae20a0566d1043587b5790a6",
  "action": "BootNotification",
  "payload": {
    "firmwareVersion": "1.0.0.1",
    "chargePointModel": "CP_EM",
    "chargePointVendor": "Apostol",
    "chargePointSerialNumber": "202206040001"
  },
  "account": "AC0001"
}
```

Example response:
```json
{
  "identity": "EM-A0000001",
  "uniqueId": "25cf07c9ae20a0566d1043587b5790a6",
  "action": "BootNotification",
  "payload": {
    "status": "Accepted",
    "interval": 600,
    "currentTime": "2024-10-22T23:08:58.205Z"
  }
}
```

### PostgreSQL

Another way to integrate the Central System is through direct connection to a PostgreSQL database.

For integration via PostgreSQL, you need to create the `ocpp` schema and several functions in the database:
- ocpp.Parse
- ocpp.ParseXML
- ocpp.ChargePointList
- ocpp.TransactionList
- ocpp.ReservationList
- ocpp.JSONToSOAP
- ocpp.SOAPToJSON

When communicating with charging stations, the Central System will call these functions and pass data from the charging stations in JSON format directly to the database. Data parsing and business logic implementation will be performed in PostgreSQL using PL/pgSQL.

#### Note: The repository version is configured for integration with the database.

To build the Central System without database integration, set the following in `CMakeLists.txt`:
```
WITH_POSTGRESQL OFF
```

## Charging Station Emulator

The Central System can create charging station emulators, which is very useful during development.

Settings for the emulators are located in the `conf/cp/` folder. Inside `cp`, there are folders with emulator settings in the form of `configuration.json` files, which contain the configuration of the charging station emulator.

You can enable emulation mode in the configuration file `conf/default.json`:

```json
{
  "module": {
    "ChargePoint": {"enable": true}
  }
}
```

If you disable the `master` process in the settings, the application will only operate in charging station emulator mode (Central System will be disabled).

```json
{
  "process": {
    "master": false
  }
}
```

## Service Management

###### If `INSTALL_AS_ROOT` is set to `ON`.

`cs` is a Linux system service (daemon).

```shell
sudo systemctl start  cs
sudo systemctl stop   cs
sudo systemctl status cs
```

## Signal Management

You can manage `cs` using signals. The main process number is recorded by default in the `/run/cs.pid` file.

| Signal | Action |
|--------|--------|
| TERM, INT | fast shutdown |
| QUIT | graceful shutdown |
| HUP | configuration reload, start new worker processes with the new configuration, graceful shutdown of old worker processes |
| WINCH | graceful shutdown of worker processes |
