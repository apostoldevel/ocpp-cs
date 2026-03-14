[![en](https://img.shields.io/badge/lang-en-green.svg)](README.md)

![image](https://user-images.githubusercontent.com/91010417/230783150-ea57f6c4-ba8a-43cf-a033-ef5ffa3a19ff.png)

# OCPP Central System

**OCPP Central System** — Центральная система и эмулятор зарядных станций на C++20.

Построена на [A-POST-OL](https://github.com/apostoldevel/libapostol) (libapostol) — высокопроизводительном фреймворке C++20 с единым циклом событий `epoll` для HTTP, WebSocket и PostgreSQL.

## Описание

**OCPP Central System** — это и готовое решение, которое вы легко можете интегрировать в ваш проект, и набор инструментов для создания приложений, работающих по протоколу OCPP.

Данное решение можно использовать для:
- Разработки или интеграции центральной системы зарядных станций;
- Эмуляции зарядных станций;
- Разработки прошивки зарядных станций.

## OCPP

Open Charge Point Protocol [OCPP](http://ocppforum.net) — это протокол связи между зарядными станциями («зарядными точками») и единой управляющей системой («центральной системой»).

**OCPP Central System** поддерживает все команды для версий протокола OCPP 1.5 и 1.6.

Версия 1.5 использует SOAP поверх HTTP в качестве RPC/транспортного протокола. Версия 1.6 использует SOAP и JSON поверх протокола WebSocket.

## API

Мы используем OpenAPI для взаимодействия с Центральной системой. Вы можете напрямую открыть Swagger UI по адресу [http://cs.ocpp-css.com/docs](http://cs.ocpp-css.com/docs).

Кроме того, вы можете использовать любой клиент OpenAPI, чтобы импортировать файл [api.yaml](https://github.com/apostoldevel/ocpp-cs/blob/master/www/docs/api.yaml) из нашего репозитория.

## Демонстрация

Вы можете подключить свою зарядную станцию к демонстрационной версии Центральной системы.

---
Адреса подключения:
- WebSocket: ws://ws.ocpp-css.com/ocpp
- SOAP: http://soap.ocpp-css.com/Ocpp
---

Для управления зарядной станцией используйте веб-интерфейс по адресу:
- [http://cs.ocpp-css.com](http://cs.ocpp-css.com)

Авторизация:
```
username: demo
password: demo
```

RFID-карта:
```
idTag: demo
```

## Сборка и установка

Самый простой способ установки Центральной системы — в виде контейнера.

### Docker Hub

```shell
docker pull apostoldevel/cs
```

Запустить контейнер:
```shell
docker run -p 9220:9220 --network host --env WEBHOOK_URL=https://api.ocpp-css.com/api/v1/ocpp/parse --rm --name cs apostoldevel/cs
```

### Сборка образа контейнера

Клонируйте репозиторий:
```shell
git clone https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs
```

Выполните настройки в соответствии с вашими требованиями:

- Отредактируйте файл `./docker/conf/default.json`, обратите особое внимание на раздел `"webhook"`;
- Отредактируйте файл `./docker/www/config.js`, укажите доменное имя или IP-адрес сервера;
- Отредактируйте файл `./docker/conf/sites/default.json`, добавьте IP-адрес вашего сервера:

  Например, IP-адрес вашего сервера `192.168.1.100` или DNS-имя `cs.example.com`.
  ```json
  {
    "hosts": ["cs.example.com", "cs.example.com:9220", "192.168.1.100:9220", "localhost:9220"]
  }
  ```

Создайте и запустите контейнер одной командой:
```shell
docker compose up
```

### Веб-приложение

После запуска контейнера Центральная система будет доступна по адресу http://localhost:9220 в вашем браузере.

Swagger UI также будет доступен по адресу http://localhost:9220/docs/.

###### Запуск из контейнера не потребует авторизации.

### Сборка из исходных кодов

#### Требования

- Компилятор **C++20**: GCC 12+ или Clang 16+
- **CMake** 3.25+
- `libssl-dev`, `libpq-dev`
- **PostgreSQL** 12+ (опционально — можно отключить)

Чтобы установить компилятор C++ и необходимые библиотеки в Debian/Ubuntu:
```shell
sudo apt-get install build-essential libssl-dev libcurl4-openssl-dev make cmake gcc g++
```

Клонируйте репозиторий:
```shell
git clone https://github.com/apostoldevel/ocpp-cs.git && cd ocpp-cs
```

#### Сборка

```shell
# Настройка (запускает cmake)
./configure               # релизная сборка
./configure --debug       # отладочная сборка

# Сборка
cmake --build cmake-build-release --parallel $(nproc)

# Установка
sudo cmake --install cmake-build-release
```

По умолчанию **cs** будет установлен в `/usr/sbin/`. Файлы конфигурации — в `/etc/cs/`.

#### Локальная разработка

```shell
mkdir -p logs
./cmake-build-debug/cs -p . -c conf/default.json
```

#### Параметры CMake

| Параметр | По умолчанию | Описание |
|----------|-------------|----------|
| `INSTALL_AS_ROOT` | ON | Установить как root. Отключить для локальной установки. |
| `WITH_POSTGRESQL` | ON | Поддержка PostgreSQL для промышленной интеграции. Отключить для автономного эмулятора. |
| `WITH_SSL` | ON | TLS, проверка JWT, авторизация OAuth 2.0. |

Чтобы собрать без интеграции с базой данных:
```cmake
WITH_POSTGRESQL OFF
WITH_SSL OFF
```

## Интеграция

Существует несколько способов интеграции Центральной системы с вашим проектом.

### Webhook

Самый простой способ — через Webhook endpoint.

В файле настроек Центральной системы `conf/default.json` имеется раздел `"webhook"`:

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

В этом разделе вы можете указать endpoint URL, на который Центральная система будет направлять пакеты, поступающие от зарядных станций.

В частности, это десять команд из раздела **4. Operations Initiated by Charge Point** спецификации OCPP v1.6:

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

Дополнительно можно настроить параметры авторизации на стороне вашего сервера, который будет принимать запросы от Центральной системы.

Данные от Центральной системы будут в следующем JSON-формате:
```json
{
  "identity": "string",
  "uniqueId": "string",
  "action": "string",
  "payload": "JSON Object",
  "account": "string"
}
```
Где:
- `identity`: Обязательный. Идентификатор зарядной станции;
- `uniqueId`: Обязательный. Идентификатор пакета данных (запроса);
- `action`: Обязательный. Имя действия;
- `payload`: Обязательный. Полезная нагрузка — данные от зарядной станции;
- `account`: Необязательный. Идентификатор учётной записи пользователя в вашей системе.

###### Благодаря `account` можно связать зарядную станцию с пользователем в системе, если бизнес-логика проекта это предусматривает. Обычно в настройках зарядной станции указывается адрес для подключения к центральной системе в формате `ws://host/ocpp/EM-A0000001`. Если к идентификатору зарядной станции `EM-A0000001` добавить дополнительное значение, например: `/EM-A0000001/AC0001`, то `AC0001` это и будет идентификатор учётной записи пользователя.

Центральная система будет ожидать ответ от вашей информационной системы в таком же JSON-формате. Значения полей (`identity`, `uniqueId`, `action`) должны быть заполнены значениями из входящего запроса, но в `payload` должны находиться данные ответа на `action` в формате спецификации протокола OCPP.

Пример запроса:
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

Пример ответа:
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

Ещё один способ интеграции Центральной системы — через прямое подключение к базе данных PostgreSQL.

Для интеграции через PostgreSQL вам нужно создать схему `ocpp` и несколько функций в базе данных:
- ocpp.Parse
- ocpp.ParseXML
- ocpp.ChargePointList
- ocpp.TransactionList
- ocpp.ReservationList
- ocpp.JSONToSOAP
- ocpp.SOAPToJSON

При коммуникации с зарядными станциями Центральная система будет вызывать эти функции и передавать данные от зарядных станций в JSON-формате непосредственно в базу данных. Разбор данных и реализация бизнес-логики будет выполняться в PostgreSQL на языке PL/pgSQL.

#### Внимание: Версия из репозитория настроена на интеграцию с базой данных.

Чтобы собрать Центральную систему без интеграции с базой данных, установите в `CMakeLists.txt`:
```
WITH_POSTGRESQL OFF
```

## Эмулятор зарядных станций

Центральная система может создавать эмуляторы зарядных станций. Очень полезно в процессе разработки.

Настройки для эмуляторов находятся в папке `conf/cp/`. Внутри `cp` находятся папки с настройками эмуляторов в виде файлов `configuration.json`, которые содержат конфигурацию зарядной станции-эмулятора.

Включить режим эмуляции можно в файле настроек `conf/default.json`:

```json
{
  "module": {
    "ChargePoint": {"enable": true}
  }
}
```

Если в настройках отключить `master`-процесс, то приложение будет работать только в режиме эмулятора зарядных станций (Центральная система будет отключена).

```json
{
  "process": {
    "master": false
  }
}
```

## Управление сервисом

###### Если `INSTALL_AS_ROOT` установлен в `ON`.

`cs` — это системная служба Linux (демон).

```shell
sudo systemctl start  cs
sudo systemctl stop   cs
sudo systemctl status cs
```

## Управление сигналами

Управлять `cs` можно с помощью сигналов. Номер главного процесса по умолчанию записывается в файл `/run/cs.pid`.

| Сигнал | Действие |
|--------|----------|
| TERM, INT | быстрое завершение |
| QUIT | плавное завершение |
| HUP | изменение конфигурации, запуск новых рабочих процессов с новой конфигурацией, плавное завершение старых |
| WINCH | плавное завершение рабочих процессов |
