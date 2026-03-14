[![en](https://img.shields.io/badge/lang-en-green.svg)](README.md)

![image](https://user-images.githubusercontent.com/91010417/230783150-ea57f6c4-ba8a-43cf-a033-ef5ffa3a19ff.png)

# OCPP Central System

**Центральная система и эмулятор зарядных станций для OCPP 1.5 (SOAP), 1.6 и 2.0.1 (JSON/WebSocket) на C++20.**

Построена на [A-POST-OL](https://github.com/apostoldevel/libapostol) — высокопроизводительном фреймворке C++20 с единым циклом событий `epoll` для HTTP, WebSocket и PostgreSQL.

## Быстрый старт

**Одна команда:**
```shell
curl -fsSL https://raw.githubusercontent.com/apostoldevel/ocpp-cs/master/install.sh | bash
```

Или вручную:
```shell
git clone --recursive https://github.com/apostoldevel/ocpp-cs.git
cd ocpp-cs
docker compose up
```

Откройте в браузере:
- **http://localhost:9220** — Веб-интерфейс (без авторизации)
- **http://localhost:9220/docs/** — Swagger UI (REST API)

**Подключите свою зарядную станцию** — укажите адрес Центральной системы в настройках станции:
```
ws://IP_ВАШЕГО_СЕРВЕРА:9220/ocpp/ID_ВАШЕЙ_СТАНЦИИ
```
Станция автоматически появится в веб-интерфейсе после подключения. Версия протокола (1.6 или 2.0.1) определяется по заголовку `Sec-WebSocket-Protocol`, который отправляет станция.

Это всё. Контейнер запускает полнофункциональную Центральную систему со встроенным эмулятором зарядных станций — без базы данных, без внешних сервисов, без настройки.

## Что вы получаете

| Возможность | Детали |
|-------------|--------|
| Центральная система | OCPP 1.5 (SOAP/HTTP), 1.6 и 2.0.1 (JSON/WebSocket) |
| Валидация по схемам | Все входящие сообщения проверяются по официальным OCPP JSON Schema |
| Эмулятор станций | Встроенные станции OCPP 1.6 и 2.0.1, автоматическое подключение при запуске |
| Веб-интерфейс | Дашборд, управление станциями, команды OCPP, лог сообщений в реальном времени |
| REST API | OpenAPI-спецификация + Swagger UI на `/docs/` |
| Установка одной командой | `curl \| bash` — Docker за 30 секунд |
| Интеграция | Webhook или PostgreSQL — на ваш выбор |

## Поддержка OCPP 2.0.1

Центральная система поддерживает 13 сообщений OCPP 2.0.1, покрывающих полный цикл зарядной сессии:

| Направление | Сообщения |
|-------------|-----------|
| CP → CSMS | BootNotification, Heartbeat, StatusNotification, Authorize, TransactionEvent, MeterValues |
| CSMS → CP | RequestStartTransaction, RequestStopTransaction, Reset, SetVariables, GetVariables, ChangeAvailability |
| Оба | DataTransfer |

Ключевые отличия от OCPP 1.6:
- **3-уровневая модель** — Станция → EVSE → Коннектор (вместо плоского списка коннекторов)
- **TransactionEvent** — одно сообщение заменяет StartTransaction, StopTransaction и MeterValues
- **Device Model** — SetVariables/GetVariables заменяют ChangeConfiguration/GetConfiguration
- **Валидация по схемам** — все сообщения проверяются по официальным OCPP 2.0.1 JSON Schema

Встроенный эмулятор включает 5 станций OCPP 2.0.1 с разными конфигурациями:

| Станция | Модель | EVSE | Коннекторы |
|---------|--------|------|------------|
| CP201 | Virtual-201 | 2 | CCS2, CCS1 |
| CP202 | SingleDC-201 | 1 | CCS1 |
| CP203 | TripleDC-201 | 3 | CCS2, CCS1, ChaoJi |
| CP204 | DualCable-201 | 2 | по 2 на EVSE (CCS2+CCS1, CCS2+ChaoJi) |
| CP205 | QuadAC-201 | 4 | Type2, Type2, Type1, Type1 |

## Демонстрация

Подключите свою зарядную станцию к демо-серверу:

| Протокол | Адрес |
|----------|-------|
| WebSocket (OCPP 1.6) | `ws://ws.ocpp-css.com/ocpp` |
| SOAP (OCPP 1.5) | `http://soap.ocpp-css.com/Ocpp` |

Веб-интерфейс: [http://cs.ocpp-css.com](http://cs.ocpp-css.com) (логин: `demo` / `demo`, RFID: `demo`)

## Docker

### Быстрый запуск (Docker Hub)

```shell
docker pull apostoldevel/cs
docker run -p 9220:9220 --rm --name cs apostoldevel/cs
```

### Сборка и запуск локально

```shell
git clone --recursive https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs
docker compose up
```

### Пользовательская настройка

Перед сборкой можно отредактировать:

| Файл | Назначение |
|------|------------|
| `docker/conf/default.json` | Настройки сервера, webhook |
| `docker/www/config.js` | Адрес сервера для веб-интерфейса |
| `docker/conf/sites/default.json` | Разрешённые имена хостов |

Пример `sites/default.json` для вашего сервера:
```json
{
  "hosts": ["cs.example.com", "cs.example.com:9220", "192.168.1.100:9220", "localhost:9220"]
}
```

## Сборка из исходных кодов

### Требования

- Компилятор **C++20**: GCC 12+ или Clang 16+
- **CMake** 3.25+
- `libssl-dev`, `zlib1g-dev`
- `libpq-dev` (опционально, только с `WITH_POSTGRESQL`)

```shell
sudo apt-get install build-essential libssl-dev zlib1g-dev make cmake gcc g++
```

### Сборка

```shell
git clone --recursive https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs

./configure               # релизная сборка
cmake --build cmake-build-release --parallel $(nproc)
sudo cmake --install cmake-build-release
```

### Локальная разработка

```shell
./configure --debug
cmake --build cmake-build-debug --parallel $(nproc)
mkdir -p logs
./cmake-build-debug/cs -p . -c conf/default.json
```

### Параметры CMake

| Параметр | По умолчанию | Описание |
|----------|-------------|----------|
| `INSTALL_AS_ROOT` | ON | Установка в системные каталоги (`/usr/sbin/`, `/etc/cs/`) |
| `WITH_POSTGRESQL` | ON | Интеграция с PostgreSQL. Отключить для автономного режима |
| `WITH_SSL` | ON | TLS, JWT, OAuth 2.0 |

Автономная сборка (без базы данных):
```shell
./configure --release --without-postgresql --without-ssl
```

## Интеграция

### Webhook

Самый простой способ интеграции. Настройка в `conf/default.json`:

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

Центральная система пересылает все сообщения от зарядных станций (Authorize, BootNotification, StartTransaction, StopTransaction, TransactionEvent и т.д.) на ваш endpoint в формате JSON:

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

Ваш сервер отвечает в том же формате с OCPP-совместимым `payload`:

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

**Поля:**
- `identity` — идентификатор зарядной станции
- `uniqueId` — идентификатор запроса
- `action` — имя действия OCPP
- `payload` — данные OCPP
- `account` — необязательный идентификатор учётной записи (извлекается из URL подключения: `ws://host/ocpp/EM-A0000001/AC0001`)

### PostgreSQL

Для прямой интеграции с базой данных создайте схему `ocpp` со следующими функциями:

- `ocpp.Parse`, `ocpp.ParseXML`
- `ocpp.ChargePointList`, `ocpp.TransactionList`, `ocpp.ReservationList`
- `ocpp.JSONToSOAP`, `ocpp.SOAPToJSON`

Центральная система вызывает эти функции при коммуникации с зарядными станциями, передавая данные в формате JSON. Вся бизнес-логика реализуется на PL/pgSQL.

## Эмулятор зарядных станций

Встроенный эмулятор создаёт виртуальные зарядные станции для разработки и тестирования.

Настройки эмуляторов находятся в `conf/cp/` — каждая подпапка содержит `configuration.json` для одной эмулируемой станции.

### Станции OCPP 1.6

Конфигурации по умолчанию создают 4 станции (`CP1`–`CP4`) с плоским списком коннекторов. Используется массив `ConnectorIds`.

### Станции OCPP 2.0.1

Станция `CP201` демонстрирует 3-уровневую модель с EVSE и коннекторами:

```json
{
  "OcppVersion": "2.0.1",
  "ChargePointVendor": "Emulator",
  "ChargePointModel": "Virtual-201",
  "Evses": [
    {"evseId": 1, "connectors": [{"connectorId": 1, "type": "cCCS2"}]},
    {"evseId": 2, "connectors": [{"connectorId": 1, "type": "cCCS1"}]}
  ]
}
```

Установите `"OcppVersion": "2.0.1"` для использования протокола OCPP 2.0.1. Если не указано — по умолчанию `"1.6"`.

### Настройка

Включение в `conf/default.json`:
```json
{
  "module": {
    "ChargePoint": {"enable": true}
  }
}
```

Режим «только эмулятор» (Центральная система отключена):
```json
{
  "process": {
    "master": false
  }
}
```

## Управление сервисом

```shell
sudo systemctl start  cs
sudo systemctl stop   cs
sudo systemctl status cs
```

### Сигналы

| Сигнал | Действие |
|--------|----------|
| TERM, INT | быстрое завершение |
| QUIT | плавное завершение |
| HUP | перезагрузка конфигурации, запуск новых рабочих процессов |
| WINCH | плавное завершение рабочих процессов |
