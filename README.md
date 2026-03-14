[![ru](https://img.shields.io/badge/lang-ru-green.svg)](README.ru-RU.md)

![image](https://user-images.githubusercontent.com/91010417/230783150-ea57f6c4-ba8a-43cf-a033-ef5ffa3a19ff.png)

# OCPP Central System

**Central system and charge point emulator for OCPP 1.5 (SOAP) and 1.6 (JSON/WebSocket) in C++20.**

Built on [A-POST-OL](https://github.com/apostoldevel/libapostol) — a high-performance C++20 framework with a single `epoll` event loop for HTTP, WebSocket, and PostgreSQL.

## Quick Start

```shell
git clone --recursive https://github.com/apostoldevel/ocpp-cs.git
cd ocpp-cs
docker compose up
```

Open in your browser:
- **http://localhost:9220** — Web UI (no login required)
- **http://localhost:9220/docs/** — Swagger UI (REST API)

That's it. The container runs a fully functional Central System with a built-in charge point emulator — no database, no external services, no configuration needed.

## What You Get

| Feature | Details |
|---------|---------|
| Central System | Full OCPP 1.5 (SOAP/HTTP) and 1.6 (JSON/WebSocket) support |
| Charge Point Emulator | Built-in, auto-connects to the Central System on startup |
| Web UI | Monitor stations, start/stop charging, view transactions |
| REST API | OpenAPI spec + Swagger UI at `/docs/` |
| Integration | Webhook or PostgreSQL — your choice |

## Live Demo

Connect your charge point to the demo server:

| Protocol | Address |
|----------|---------|
| WebSocket (OCPP 1.6) | `ws://ws.ocpp-css.com/ocpp` |
| SOAP (OCPP 1.5) | `http://soap.ocpp-css.com/Ocpp` |

Web UI: [http://cs.ocpp-css.com](http://cs.ocpp-css.com) (login: `demo` / `demo`, RFID: `demo`)

## Docker

### Quick Run (Docker Hub)

```shell
docker pull apostoldevel/cs
docker run -p 9220:9220 --rm --name cs apostoldevel/cs
```

### Build & Run Locally

```shell
git clone --recursive https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs
docker compose up
```

### Custom Configuration

Before building, you can edit:

| File | Purpose |
|------|---------|
| `docker/conf/default.json` | Server settings, webhook endpoint |
| `docker/www/config.js` | Web UI server address |
| `docker/conf/sites/default.json` | Allowed hostnames |

Example `sites/default.json` for your server:
```json
{
  "hosts": ["cs.example.com", "cs.example.com:9220", "192.168.1.100:9220", "localhost:9220"]
}
```

## Build from Source

### Prerequisites

- **C++20** compiler: GCC 12+ or Clang 16+
- **CMake** 3.25+
- `libssl-dev`, `zlib1g-dev`
- `libpq-dev` (optional, only with `WITH_POSTGRESQL`)

```shell
sudo apt-get install build-essential libssl-dev zlib1g-dev make cmake gcc g++
```

### Build

```shell
git clone --recursive https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs

./configure               # release build
cmake --build cmake-build-release --parallel $(nproc)
sudo cmake --install cmake-build-release
```

### Local Development

```shell
./configure --debug
cmake --build cmake-build-debug --parallel $(nproc)
mkdir -p logs
./cmake-build-debug/cs -p . -c conf/default.json
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `INSTALL_AS_ROOT` | ON | Install to system dirs (`/usr/sbin/`, `/etc/cs/`) |
| `WITH_POSTGRESQL` | ON | PostgreSQL integration. Disable for standalone mode |
| `WITH_SSL` | ON | TLS, JWT, OAuth 2.0 |

Standalone build (no database):
```shell
./configure --release --without-postgresql --without-ssl
```

## Integration

### Webhook

The simplest integration method. Configure in `conf/default.json`:

```json
{
  "webhook": {
    "enable": true,
    "url": "http://your-server/api/v1/ocpp/parse",
    "authorization": "Basic",
    "username": "ocpp",
    "password": "ocpp"
  }
}
```

The Central System forwards all charge point-initiated messages (Authorize, BootNotification, StartTransaction, StopTransaction, etc.) to your endpoint as JSON:

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

Your server responds in the same format with the OCPP-compliant `payload`:

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

**Fields:**
- `identity` — charge point identifier
- `uniqueId` — request ID
- `action` — OCPP action name
- `payload` — OCPP data
- `account` — optional user account (extracted from the connection URL: `ws://host/ocpp/EM-A0000001/AC0001`)

### PostgreSQL

For direct database integration, create the `ocpp` schema with these functions:

- `ocpp.Parse`, `ocpp.ParseXML`
- `ocpp.ChargePointList`, `ocpp.TransactionList`, `ocpp.ReservationList`
- `ocpp.JSONToSOAP`, `ocpp.SOAPToJSON`

The Central System calls these functions during charge point communication, passing data in JSON format. All business logic is implemented in PL/pgSQL.

## Charge Point Emulator

The built-in emulator creates virtual charge points for development and testing.

Emulator configs are in `conf/cp/` — each subfolder contains a `configuration.json` for one emulated station.

Enable in `conf/default.json`:
```json
{
  "module": {
    "ChargePoint": {"enable": true}
  }
}
```

Emulator-only mode (disable Central System):
```json
{
  "process": {
    "master": false
  }
}
```

## Service Management

```shell
sudo systemctl start  cs
sudo systemctl stop   cs
sudo systemctl status cs
```

### Signals

| Signal | Action |
|--------|--------|
| TERM, INT | fast shutdown |
| QUIT | graceful shutdown |
| HUP | reload configuration, start new workers |
| WINCH | graceful worker shutdown |
