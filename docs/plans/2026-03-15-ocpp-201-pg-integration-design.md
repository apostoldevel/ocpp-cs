# OCPP 2.0.1 PostgreSQL Integration — Design Document

**Date:** 2026-03-15
**Status:** Approved
**Prereq:** Phase 1 MVP + Phase 2 Audit complete (22 messages, SchemaRegistry, CPEmulator 2.0.1)

---

## 1. Goal

Connect OCPP 2.0.1 message handling to PostgreSQL — the same `ocpp.parse()` pipeline used by OCPP 1.6. All 9 CP→CSMS messages get full PG business logic: station registration, authorization, transaction lifecycle, meter values, status tracking.

## 2. Approach

- **Same `ocpp.parse()` function** — add `pVersion` parameter, branch internally
- **Extend existing tables** — add nullable fields for 2.0.1 (evse_id, transaction_sid, ocpp_version)
- **New PG handler functions** — `ocpp.BootNotification201`, `ocpp.StatusNotification201`, `ocpp.Authorize201`, `ocpp.TransactionEvent`, `ocpp.MeterValues201`, `ocpp.NotifyReport`
- **Reuse existing handlers** where format is identical — `Heartbeat`, `DataTransfer`, `FirmwareStatusNotification`

## 3. C++ Changes — CSService

### 3.1 Unified routing in on_ws_message()

**Current (line 361-375):**
```cpp
if (point.ocpp_version() == "2.0.1") {
    handle_action_201(point, msg);
} else {
    if (pool_) parse_json_pg(point, msg);
    else if (webhook_.enabled) parse_json_webhook(point, msg);
    else parse_json_standalone(point, msg);
}
```

**New:**
```cpp
#ifdef WITH_POSTGRESQL
if (pool_) {
    parse_json_pg(point, msg);
} else
#endif
if (webhook_.enabled) {
    parse_json_webhook(point, msg);
} else {
    if (point.ocpp_version() == "2.0.1")
        handle_action_201(point, msg);
    else
        parse_json_standalone(point, msg);
}
```

Both 1.6 and 2.0.1 go through PG when pool_ is available. `handle_action_201()` becomes the standalone fallback for 2.0.1.

### 3.2 Version parameter in parse_json_pg()

**Current SQL:**
```cpp
auto sql = fmt::format(
    "SELECT * FROM ocpp.parse({}, {}, {}, {}::jsonb, {})",
    pq_quote_literal(point.identity()),
    pq_quote_literal(msg.unique_id),
    pq_quote_literal(msg.action),
    pq_quote_literal(msg.payload.dump()),
    pq_quote_literal(account));
```

**New SQL:**
```cpp
auto sql = fmt::format(
    "SELECT * FROM ocpp.parse({}, {}, {}, {}::jsonb, {}, {})",
    pq_quote_literal(point.identity()),
    pq_quote_literal(msg.unique_id),
    pq_quote_literal(msg.action),
    pq_quote_literal(msg.payload.dump()),
    pq_quote_literal(account),
    pq_quote_literal(point.ocpp_version()));
```

### 3.3 Files affected

| File | Changes |
|------|---------|
| `src/modules/Workers/CSService/CSService.cpp` | on_ws_message routing, parse_json_pg version param |

---

## 4. Data Model — Table Extensions

All new fields are nullable — zero impact on existing 1.6 data.

### 4.1 patch.sql additions

```sql
-- db.charge_point: track OCPP version
ALTER TABLE db.charge_point ADD COLUMN IF NOT EXISTS ocpp_version text DEFAULT '1.6';

-- db.connector: EVSE layer for 2.0.1
ALTER TABLE db.connector ADD COLUMN IF NOT EXISTS evse_id integer;

-- db.charge_point_transaction: string transaction ID for 2.0.1
ALTER TABLE db.charge_point_transaction ADD COLUMN IF NOT EXISTS transaction_sid text;
ALTER TABLE db.charge_point_transaction ADD COLUMN IF NOT EXISTS evse_id integer;
CREATE INDEX IF NOT EXISTS idx_cpt_transaction_sid ON db.charge_point_transaction (transaction_sid) WHERE transaction_sid IS NOT NULL;

-- db.status_notification: 2.0.1 fields
ALTER TABLE db.status_notification ADD COLUMN IF NOT EXISTS evse_id integer;
ALTER TABLE db.status_notification ADD COLUMN IF NOT EXISTS connector_status text;

-- db.meter_value: EVSE layer for 2.0.1
ALTER TABLE db.meter_value ADD COLUMN IF NOT EXISTS evse_id integer;
```

### 4.2 Table summary

| Table | New Column | Type | Purpose |
|-------|-----------|------|---------|
| `db.charge_point` | `ocpp_version` | `text DEFAULT '1.6'` | Protocol version |
| `db.connector` | `evse_id` | `integer` | EVSE ID (NULL for 1.6) |
| `db.charge_point_transaction` | `transaction_sid` | `text` | String transactionId for 2.0.1 |
| `db.charge_point_transaction` | `evse_id` | `integer` | EVSE ID |
| `db.status_notification` | `evse_id` | `integer` | EVSE ID |
| `db.status_notification` | `connector_status` | `text` | 2.0.1 ConnectorStatusEnumType |
| `db.meter_value` | `evse_id` | `integer` | EVSE ID |

---

## 5. PG Functions — ocpp.Parse() Extension

### 5.1 New signature

```sql
CREATE OR REPLACE FUNCTION ocpp.Parse (
  pIdentity     text,
  pUniqueId     text,
  pAction       text,
  pPayload      jsonb,
  pAccount      text DEFAULT null,
  pVersion      text DEFAULT '1.6'
) RETURNS       json
```

### 5.2 Version branching

```sql
IF pVersion = '2.0.1' THEN
    CASE pAction
    WHEN 'BootNotification' THEN
        jPayload := ocpp.BootNotification201(pIdentity, pPayload, pAccount);
    WHEN 'Heartbeat' THEN
        jPayload := ocpp.Heartbeat(pIdentity, pPayload);
    WHEN 'StatusNotification' THEN
        jPayload := ocpp.StatusNotification201(pIdentity, pPayload);
    WHEN 'Authorize' THEN
        jPayload := ocpp.Authorize201(pIdentity, pPayload);
    WHEN 'TransactionEvent' THEN
        jPayload := ocpp.TransactionEvent(pIdentity, pPayload);
    WHEN 'MeterValues' THEN
        jPayload := ocpp.MeterValues201(pIdentity, pPayload);
    WHEN 'DataTransfer' THEN
        jPayload := ocpp.DataTransfer(pIdentity, pPayload);
    WHEN 'NotifyReport' THEN
        jPayload := ocpp.NotifyReport(pIdentity, pPayload);
    WHEN 'FirmwareStatusNotification' THEN
        jPayload := ocpp.FirmwareStatusNotification(pIdentity, pPayload);
    ELSE
        PERFORM ActionNotFound(pAction);
    END CASE;
ELSE
    -- existing 1.6 CASE (unchanged)
END IF;
```

---

## 6. New Handler Functions

### 6.1 ocpp.BootNotification201

**Payload format:**
```json
{
  "chargingStation": {
    "model": "Virtual-201",
    "vendorName": "Emulator",
    "serialNumber": "SN001",
    "firmwareVersion": "1.0.0"
  },
  "reason": "PowerUp"
}
```

**Logic:**
1. Extract `chargingStation.vendorName`, `chargingStation.model`, `chargingStation.serialNumber`, `chargingStation.firmwareVersion`
2. Find or create charge point (same as 1.6 `BootNotification` logic)
3. Set `ocpp_version = '2.0.1'` on `db.charge_point`
4. Return `{currentTime, interval: 60, status: "Accepted"}`

**Response:**
```json
{"currentTime": "...", "interval": 60, "status": "Accepted"}
```

### 6.2 ocpp.StatusNotification201

**Payload format:**
```json
{
  "timestamp": "2026-03-15T12:00:00Z",
  "connectorStatus": "Available",
  "evseId": 1,
  "connectorId": 1
}
```

**Logic:**
1. Find charge point by identity
2. Insert into `db.status_notification` with `evse_id`, `connector_status`
3. Execute state machine method (same pattern as 1.6)
4. Return `{}` (empty per spec)

### 6.3 ocpp.Authorize201

**Payload format:**
```json
{
  "idToken": {
    "idToken": "RFID_VALUE",
    "type": "ISO14443"
  }
}
```

**Logic:**
1. Extract `idToken.idToken` as the tag value
2. Call existing `ocpp.GetIdTagStatus()` with the tag
3. Return `{idTokenInfo: {status: "Accepted"}}` (no expiryDate in 2.0.1 default response)

**Response:**
```json
{"idTokenInfo": {"status": "Accepted"}}
```

### 6.4 ocpp.TransactionEvent — main handler

**Payload format:**
```json
{
  "eventType": "Started",
  "timestamp": "...",
  "triggerReason": "RemoteStart",
  "seqNo": 0,
  "transactionInfo": {
    "transactionId": "uuid-string",
    "chargingState": "Charging",
    "stoppedReason": "Remote",
    "remoteStartId": 123
  },
  "evse": {"id": 1, "connectorId": 1},
  "idToken": {"idToken": "RFID_VALUE", "type": "ISO14443"},
  "meterValue": [{"timestamp": "...", "sampledValue": [{"value": 1234.5}]}]
}
```

**Logic by eventType:**

| eventType | Actions |
|-----------|---------|
| **Started** | Find/validate idToken → `kernel.StartTransaction(card, chargePoint, connectorId, meterStart, null, timestamp)` with evse_id → set `transaction_sid` on record → `kernel.StartConnectorLock()` → `kernel.CreateTransactions()` → return `{idTokenInfo: {status}}` |
| **Updated** | Find transaction by `transaction_sid` → save meter values → update volume → return `{}` |
| **Ended** | Find transaction by `transaction_sid` → extract stoppedReason → `kernel.StopTransaction(id, meterStop, reason, data, timestamp)` → `kernel.UpdateTransactions()` → return `{}` |

**Key mappings:**
- `transactionInfo.transactionId` → `db.charge_point_transaction.transaction_sid`
- `evse.id` → `db.charge_point_transaction.evse_id`
- `evse.connectorId` → `db.charge_point_transaction.connectorId`
- `idToken.idToken` → card lookup via `GetCard()`
- `meterValue[0].sampledValue[0].value` → meter start/stop

**Meter start extraction for Started:**
```sql
IF pRequest->'meterValue' IS NOT NULL THEN
    meterStart := (pRequest->'meterValue'->0->'sampledValue'->0->>'value')::integer;
END IF;
```

### 6.5 ocpp.MeterValues201

**Payload format:**
```json
{
  "evseId": 1,
  "meterValue": [{"timestamp": "...", "sampledValue": [{"value": 1500.0, "measurand": "Energy.Active.Import.Register"}]}]
}
```

**Logic:**
1. Find charge point and active transaction for evse_id
2. Save meter values (same as existing `ocpp.MeterValues` but with evse_id)
3. Return `{}` (empty per spec)

### 6.6 ocpp.NotifyReport

**Payload format:**
```json
{
  "requestId": 1,
  "generatedAt": "...",
  "tbc": false,
  "seqNo": 0,
  "reportData": [...]
}
```

**Logic:**
1. Log the report data (JSON stored in ocpp.log via WriteToLog)
2. Return `{}` (empty per spec)

No persistent storage needed — NotifyReport is informational. The ocpp.log already captures the full payload.

---

## 7. Shared Functions (1.6 and 2.0.1)

These functions work identically for both versions — no changes needed:

| Function | Why shared |
|----------|-----------|
| `ocpp.Heartbeat` | Same response: `{currentTime}` |
| `ocpp.DataTransfer` | Same structure |
| `ocpp.FirmwareStatusNotification` | Same structure, empty response |

---

## 8. Helper: Find Transaction by SID

```sql
CREATE OR REPLACE FUNCTION ocpp.GetTransactionBySid(pSid text)
RETURNS integer  -- returns charge_point_transaction.id
AS $$
    SELECT id FROM db.charge_point_transaction WHERE transaction_sid = pSid LIMIT 1;
$$ LANGUAGE sql STABLE
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

---

## 9. Files Affected

### C++ (ocpp/ repo)

| File | Changes |
|------|---------|
| `src/modules/Workers/CSService/CSService.cpp` | Unified routing (remove version branch in on_ws_message), add version param to parse_json_pg SQL |

### SQL (backend/db/ repo)

| File | Changes |
|------|---------|
| `sql/configuration/chargemecar/ocpp/routine.sql` | `ocpp.Parse` +pVersion param, +6 new functions (BootNotification201, StatusNotification201, Authorize201, TransactionEvent, MeterValues201, NotifyReport), +GetTransactionBySid |
| `sql/configuration/chargemecar/patch/patch.sql` | ALTER TABLE for 5 tables (charge_point, connector, charge_point_transaction, status_notification, meter_value) |

---

## 10. Testing

### Via Docker + curl

| Test | Command | Expected |
|------|---------|----------|
| BootNotification201 | Connect 2.0.1 emulator with PG | Station appears in `db.charge_point` with `ocpp_version='2.0.1'` |
| Authorize201 | Send Authorize via emulator | idTokenInfo with status from DB |
| TransactionEvent Started | Start transaction on 2.0.1 station | Row in `db.charge_point_transaction` with `transaction_sid` |
| TransactionEvent Updated | Mid-session meter values | `db.meter_value` rows with `evse_id` |
| TransactionEvent Ended | Stop transaction | `meterStop`, `reason` populated |
| StatusNotification201 | Emulator sends status | `db.status_notification` with `evse_id` |
| MeterValues201 | TriggerMessage MeterValues | `db.meter_value` rows |

### Database verification

```sql
-- Check 2.0.1 stations
SELECT identity, ocpp_version, connected FROM db.charge_point WHERE ocpp_version = '2.0.1';

-- Check 2.0.1 transactions
SELECT id, transaction_sid, evse_id, connectorid, meterstart, meterstop FROM db.charge_point_transaction WHERE transaction_sid IS NOT NULL;

-- Check meter values with EVSE
SELECT * FROM db.meter_value WHERE evse_id IS NOT NULL ORDER BY validfromdate DESC LIMIT 10;
```
