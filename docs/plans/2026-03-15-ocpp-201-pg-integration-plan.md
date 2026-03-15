# OCPP 2.0.1 PostgreSQL Integration — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Connect OCPP 2.0.1 message handling to PostgreSQL — all 9 CP→CSMS messages get full PG business logic through `ocpp.parse()`.

**Architecture:** Extend existing `ocpp.parse()` with `pVersion` parameter, add nullable EVSE/2.0.1 fields to existing tables, create 6 new PL/pgSQL handler functions (3 shared with 1.6). C++ routing unified so both 1.6 and 2.0.1 go through PG when pool_ is available.

**Tech Stack:** C++20 (libapostol), PostgreSQL (PL/pgSQL), nlohmann/json

**Design doc:** `docs/plans/2026-03-15-ocpp-201-pg-integration-design.md`

---

## Task 1: Database Schema Patch — Add 2.0.1 Columns

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/patch/patch.psql`
- Create: `backend/db/sql/configuration/chargemecar/patch/P00000002.sql`

**Step 1: Create patch file**

Create `backend/db/sql/configuration/chargemecar/patch/P00000002.sql`:

```sql
-- P00000002.sql — OCPP 2.0.1 support columns
-- Run: ./runme.sh --patch

-- db.charge_point: track OCPP version
ALTER TABLE db.charge_point ADD COLUMN IF NOT EXISTS ocpp_version text DEFAULT '1.6';

-- db.connector: EVSE layer for 2.0.1 (3-tier model: Station → EVSE → Connector)
ALTER TABLE db.connector ADD COLUMN IF NOT EXISTS evse_id integer;

-- db.charge_point_transaction: string transaction ID + EVSE for 2.0.1
ALTER TABLE db.charge_point_transaction ADD COLUMN IF NOT EXISTS transaction_sid text;
ALTER TABLE db.charge_point_transaction ADD COLUMN IF NOT EXISTS evse_id integer;
CREATE INDEX IF NOT EXISTS idx_cpt_transaction_sid ON db.charge_point_transaction (transaction_sid) WHERE transaction_sid IS NOT NULL;

-- db.status_notification: 2.0.1 fields
ALTER TABLE db.status_notification ADD COLUMN IF NOT EXISTS evse_id integer;
ALTER TABLE db.status_notification ADD COLUMN IF NOT EXISTS connector_status text;

-- db.meter_value: EVSE layer for 2.0.1
ALTER TABLE db.meter_value ADD COLUMN IF NOT EXISTS evse_id integer;
```

**Step 2: Enable patch in patch.psql**

In `backend/db/sql/configuration/chargemecar/patch/patch.psql`, uncomment/add:

```sql
\ir P00000002.sql
```

**Step 3: Run the patch**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db
./runme.sh --patch
```

Expected: No errors. All ALTER TABLE statements succeed (IF NOT EXISTS is safe).

**Step 4: Verify columns exist**

```bash
psql "host=localhost port=5432 dbname=chargemecar user=admin sslmode=disable" -c "
  SELECT column_name, data_type, column_default
  FROM information_schema.columns
  WHERE table_schema = 'db' AND table_name = 'charge_point' AND column_name = 'ocpp_version';
"
```

Expected: `ocpp_version | text | '1.6'::text`

**Step 5: Commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/patch/P00000002.sql db/sql/configuration/chargemecar/patch/patch.psql
git commit -m "feat(db): add OCPP 2.0.1 columns — ocpp_version, evse_id, transaction_sid, connector_status"
```

---

## Task 2: PG Function — ocpp.BootNotification201

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/ocpp/routine.sql`

**Context:** OCPP 2.0.1 BootNotification payload uses `chargingStation` object (not flat fields like 1.6). Field mapping:
- 1.6: `chargePointVendor`, `chargePointModel`, `chargePointSerialNumber`, `firmwareVersion`
- 2.0.1: `chargingStation.vendorName`, `chargingStation.model`, `chargingStation.serialNumber`, `chargingStation.firmwareVersion`

**Step 1: Add function before ocpp.Parse()**

Insert this function before the `ocpp.Parse` function (before line 1114) in `routine.sql`:

```sql
--------------------------------------------------------------------------------
-- ocpp.BootNotification201 ---------------------------------------------------
--------------------------------------------------------------------------------
/**
 * OCPP 2.0.1 BootNotification handler.
 * @param {text} pIdentity - Charge point identity
 * @param {jsonb} pRequest - {chargingStation: {vendorName, model, serialNumber, firmwareVersion}, reason}
 * @param {text} pAccount - Optional account
 * @return {json} - {currentTime, interval, status}
 */
CREATE OR REPLACE FUNCTION ocpp.BootNotification201 (
  pIdentity                 text,
  pRequest                  jsonb,
  pAccount                  text DEFAULT null
) RETURNS                   json
AS $$
DECLARE
  uType                     uuid;
  uClient                   uuid;
  uNetwork                  uuid;
  uChargePoint              uuid;

  arKeys                    text[];
  vStatus                   text;

  jStation                  jsonb;

  vVendorName               text;
  vModel                    text;
  vSerialNumber             text;
  vFirmwareVersion          text;

  vChargePointCode          text;
  vStateCode                text;

  vMessage                  text;
  vContext                  text;
BEGIN
  IF pRequest IS NULL THEN
    PERFORM JsonIsEmpty();
  END IF;

  arKeys := array_cat(arKeys, ARRAY['chargingStation', 'reason']);
  PERFORM CheckJsonbKeys(pIdentity || '/BootNotification', arKeys, pRequest);

  jStation := pRequest->'chargingStation';

  vVendorName := NULLIF(trim(jStation->>'vendorName'), '');
  vModel := NULLIF(trim(jStation->>'model'), '');
  vSerialNumber := NULLIF(trim(jStation->>'serialNumber'), '');
  vFirmwareVersion := NULLIF(trim(jStation->>'firmwareVersion'), '');

  vChargePointCode := coalesce(vSerialNumber, pIdentity);
  uChargePoint := GetChargePoint(pIdentity);

  IF uChargePoint IS NULL THEN
    uClient := coalesce(GetClient(pAccount), GetClient('demo'));

    IF uClient IS NULL THEN
      uType := GetType('public.charge_point');
      uNetwork := GetNetwork('public.network');
    ELSE
      uType := GetType('private.charge_point');
      uNetwork := GetNetwork('private.network');
    END IF;

    uChargePoint := AddChargePoint(null, uType, uNetwork, uClient, pIdentity, vVendorName, vModel, vFirmwareVersion, vSerialNumber, null, null, null, null, null, false, pIdentity);
  END IF;

  -- Set OCPP version to 2.0.1
  UPDATE db.charge_point SET ocpp_version = '2.0.1' WHERE id = uChargePoint AND ocpp_version IS DISTINCT FROM '2.0.1';

  SELECT coalesce(serialnumber, identity) INTO vChargePointCode FROM db.charge_point WHERE id = uChargePoint;

  vStatus := 'Rejected';
  IF vChargePointCode = coalesce(vSerialNumber, pIdentity) THEN
    vStateCode := coalesce(GetObjectStateTypeCode(uChargePoint), 'deleted');
    IF vStateCode IN ('enabled', 'disabled') THEN
      vStatus := 'Accepted';
    END IF;
  END IF;

  RETURN json_build_object('currentTime', GetISOTime(), 'interval', 60, 'status', vStatus);
EXCEPTION
WHEN others THEN
  GET STACKED DIAGNOSTICS vMessage = MESSAGE_TEXT, vContext = PG_EXCEPTION_CONTEXT;

  PERFORM WriteDiagnostics(vMessage, vContext);

  RETURN json_build_object('currentTime', GetISOTime(), 'interval', 60, 'status', 'Rejected');
END;
$$ LANGUAGE plpgsql
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

**Step 2: Run update**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db
./runme.sh --update
```

Expected: No errors.

**Step 3: Verify function exists**

```bash
psql "host=localhost port=5432 dbname=chargemecar user=admin sslmode=disable" -c "
  SELECT routine_name FROM information_schema.routines WHERE routine_schema = 'ocpp' AND routine_name = 'bootnotification201';
"
```

Expected: `bootnotification201`

**Step 4: Commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/ocpp/routine.sql
git commit -m "feat(db): add ocpp.BootNotification201 — OCPP 2.0.1 station registration"
```

---

## Task 3: PG Function — ocpp.Authorize201

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/ocpp/routine.sql`

**Context:** OCPP 2.0.1 uses `idToken` object `{idToken: "RFID_VALUE", type: "ISO14443"}` instead of flat `idTag` string. Extract `idToken.idToken` as the tag value.

**Step 1: Add function after BootNotification201**

```sql
--------------------------------------------------------------------------------
-- ocpp.Authorize201 ----------------------------------------------------------
--------------------------------------------------------------------------------
/**
 * OCPP 2.0.1 Authorize handler.
 * @param {text} pIdentity - Charge point identity
 * @param {jsonb} pRequest - {idToken: {idToken: "value", type: "ISO14443"}}
 * @return {json} - {idTokenInfo: {status}}
 */
CREATE OR REPLACE FUNCTION ocpp.Authorize201 (
  pIdentity     text,
  pRequest      jsonb
) RETURNS       json
AS $$
DECLARE
  uChargePoint  uuid;

  arKeys        text[];
  vStatus       text;

  vIdToken      text;

  vMessage      text;
  vContext      text;
BEGIN
  IF pRequest IS NULL THEN
    PERFORM JsonIsEmpty();
  END IF;

  arKeys := array_cat(arKeys, ARRAY['idToken', 'certificate', '15118CertificateHashData']);
  PERFORM CheckJsonbKeys(pIdentity || '/Authorize', arKeys, pRequest);

  -- Extract idToken.idToken (nested object)
  vIdToken := pRequest->'idToken'->>'idToken';

  uChargePoint := GetChargePoint(pIdentity);

  vStatus := ocpp.GetIdTagStatus(uChargePoint, vIdToken, 'Authorize');

  RETURN json_build_object('idTokenInfo', json_build_object('status', vStatus));
EXCEPTION
WHEN others THEN
  GET STACKED DIAGNOSTICS vMessage = MESSAGE_TEXT, vContext = PG_EXCEPTION_CONTEXT;

  PERFORM WriteDiagnostics(vMessage, vContext);

  RETURN json_build_object('idTokenInfo', json_build_object('status', 'Invalid'));
END;
$$ LANGUAGE plpgsql
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

**Step 2: Run update and commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db && ./runme.sh --update
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/ocpp/routine.sql
git commit -m "feat(db): add ocpp.Authorize201 — OCPP 2.0.1 idToken authorization"
```

---

## Task 4: PG Function — ocpp.StatusNotification201

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/ocpp/routine.sql`

**Context:** OCPP 2.0.1 StatusNotification has different fields: `{timestamp, connectorStatus, evseId, connectorId}` — no `errorCode`, no `info`, no `vendorId`. The `status` field is renamed to `connectorStatus`.

**Step 1: Add function**

```sql
--------------------------------------------------------------------------------
-- ocpp.StatusNotification201 -------------------------------------------------
--------------------------------------------------------------------------------
/**
 * OCPP 2.0.1 StatusNotification handler.
 * @param {text} pIdentity - Charge point identity
 * @param {jsonb} pRequest - {timestamp, connectorStatus, evseId, connectorId}
 * @return {json} - {} (empty per spec)
 */
CREATE OR REPLACE FUNCTION ocpp.StatusNotification201 (
  pIdentity             text,
  pRequest              jsonb
) RETURNS               json
AS $$
DECLARE
  uId                   uuid;
  uModel                uuid;
  uChargePoint          uuid;
  uConnector            uuid;
  uMethod               uuid;
  uAction               uuid;

  arKeys                text[];

  nEvseId               integer;
  nConnectorId          integer;
  vConnectorStatus      text;
  tsTimestamp            timestamptz;
BEGIN
  IF pRequest IS NULL THEN
    PERFORM JsonIsEmpty();
  END IF;

  arKeys := array_cat(arKeys, ARRAY['timestamp', 'connectorStatus', 'evseId', 'connectorId']);
  PERFORM CheckJsonbKeys(pIdentity || '/StatusNotification', arKeys, pRequest);

  nEvseId := pRequest->>'evseId';
  nConnectorId := pRequest->>'connectorId';
  vConnectorStatus := pRequest->>'connectorStatus';
  tsTimestamp := coalesce((pRequest->>'timestamp')::timestamptz, current_timestamp at time zone 'utc');

  SELECT id, model INTO uChargePoint, uModel FROM db.charge_point WHERE identity = pIdentity;

  IF uChargePoint IS NOT NULL THEN
    -- Store with 2.0.1-specific fields
    uId := AddStatusNotification(uChargePoint, nConnectorId, vConnectorStatus, 'NoError', null, null, null, tsTimestamp);

    -- Update evse_id and connector_status on the record
    UPDATE db.status_notification SET evse_id = nEvseId, connector_status = vConnectorStatus WHERE id = uId;

    -- Execute state machine action (map 2.0.1 status to action)
    uAction := GetAction(lower(vConnectorStatus));

    IF nEvseId = 0 OR nConnectorId = 0 THEN
      uMethod := GetObjectMethod(uChargePoint, uAction);
      IF uMethod IS NOT NULL THEN
        PERFORM ExecuteMethod(uChargePoint, uMethod, pRequest);
      ELSE
        PERFORM ExecuteAction(GetObjectClass(uChargePoint), uAction);
      END IF;
    ELSE
      uConnector := GetConnector(uChargePoint, nConnectorId);

      IF uConnector IS NULL THEN
        uConnector := CreateConnector(uChargePoint, GetType('ac.connector'), uChargePoint, nConnectorId, GetCharger('type2.charger'), GetMode('standard.mode'));
      END IF;

      uMethod := GetObjectMethod(uConnector, uAction);
      IF uMethod IS NOT NULL THEN
        PERFORM ExecuteMethod(uConnector, uMethod, pRequest);
      ELSE
        PERFORM ExecuteAction(GetObjectClass(uConnector), uAction);
      END IF;
    END IF;
  END IF;

  RETURN json_build_object();
END;
$$ LANGUAGE plpgsql
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

**Step 2: Run update and commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db && ./runme.sh --update
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/ocpp/routine.sql
git commit -m "feat(db): add ocpp.StatusNotification201 — OCPP 2.0.1 status with evseId"
```

---

## Task 5: PG Function — ocpp.GetTransactionBySid + ocpp.TransactionEvent

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/ocpp/routine.sql`

**Context:** This is the most complex handler. `TransactionEvent` replaces `StartTransaction`, `StopTransaction`, and `MeterValues` from 1.6. It has `eventType`: Started, Updated, Ended.

**Step 1: Add helper function GetTransactionBySid**

```sql
--------------------------------------------------------------------------------
-- ocpp.GetTransactionBySid ---------------------------------------------------
--------------------------------------------------------------------------------
/**
 * Find charge_point_transaction by string transaction ID (OCPP 2.0.1).
 * @param {text} pSid - String transaction ID (e.g. UUID)
 * @return {integer} - Internal transaction ID, or NULL
 */
CREATE OR REPLACE FUNCTION ocpp.GetTransactionBySid (
  pSid          text
) RETURNS       integer
AS $$
  SELECT id FROM db.charge_point_transaction WHERE transaction_sid = pSid LIMIT 1;
$$ LANGUAGE sql STABLE
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

**Step 2: Add TransactionEvent function**

```sql
--------------------------------------------------------------------------------
-- ocpp.TransactionEvent ------------------------------------------------------
--------------------------------------------------------------------------------
/**
 * OCPP 2.0.1 TransactionEvent handler.
 * Replaces StartTransaction + StopTransaction + MeterValues from 1.6.
 * @param {text} pIdentity - Charge point identity
 * @param {jsonb} pRequest - {eventType, timestamp, triggerReason, seqNo, transactionInfo, evse, idToken, meterValue}
 * @return {json} - {idTokenInfo?} for Started, {} for Updated/Ended
 */
CREATE OR REPLACE FUNCTION ocpp.TransactionEvent (
  pIdentity         text,
  pRequest          jsonb
) RETURNS           json
AS $$
DECLARE
  uChargePoint      uuid;
  uCard             uuid;
  uClient           uuid;
  uConnector        uuid;

  arKeys            text[];
  vStatus           text;

  vEventType        text;
  vTriggerReason    text;
  vTransactionSid   text;
  vStoppedReason    text;

  nEvseId           integer;
  nConnectorId      integer;
  nTransactionId    integer;
  nMeterStart       integer;
  nMeterStop        integer;
  nRemoteStartId    integer;

  vIdToken          text;
  tsTimestamp        timestamptz;

  meterValue        json;

  vMessage          text;
  vContext          text;
BEGIN
  IF pRequest IS NULL THEN
    PERFORM JsonIsEmpty();
  END IF;

  arKeys := array_cat(arKeys, ARRAY['eventType', 'timestamp', 'triggerReason', 'seqNo',
    'transactionInfo', 'evse', 'idToken', 'meterValue', 'offline', 'numberOfPhasesUsed',
    'cableMaxCurrent', 'reservationId']);
  PERFORM CheckJsonbKeys(pIdentity || '/TransactionEvent', arKeys, pRequest);

  vEventType := pRequest->>'eventType';
  vTriggerReason := pRequest->>'triggerReason';
  tsTimestamp := coalesce((pRequest->>'timestamp')::timestamptz, current_timestamp at time zone 'utc');

  -- Extract transactionInfo
  vTransactionSid := pRequest->'transactionInfo'->>'transactionId';
  vStoppedReason := pRequest->'transactionInfo'->>'stoppedReason';
  nRemoteStartId := (pRequest->'transactionInfo'->>'remoteStartId')::integer;

  -- Extract EVSE info
  nEvseId := (pRequest->'evse'->>'id')::integer;
  nConnectorId := coalesce((pRequest->'evse'->>'connectorId')::integer, 1);

  -- Extract idToken
  IF pRequest->'idToken' IS NOT NULL THEN
    vIdToken := pRequest->'idToken'->>'idToken';
  END IF;

  -- Extract meter value (first entry, first sampledValue)
  IF pRequest->'meterValue' IS NOT NULL THEN
    meterValue := (pRequest->>'meterValue')::json;
  END IF;

  SELECT id INTO uChargePoint FROM db.charge_point WHERE identity = pIdentity;

  IF uChargePoint IS NULL THEN
    RETURN json_build_object();
  END IF;

  CASE vEventType
  WHEN 'Started' THEN

    -- Authorize idToken
    vStatus := 'Accepted';
    IF vIdToken IS NOT NULL THEN
      uCard := GetCard(vIdToken);

      IF uCard IS NULL THEN
        uCard := CreateCard(null, GetType('rfid.card'), GetClient('demo'), vIdToken);
        PERFORM DoEnable(uCard);
      END IF;

      uClient := GetCardClient(uCard);
      vStatus := ocpp.GetIdTagStatus(uChargePoint, vIdToken, 'StartTransaction');
    ELSE
      -- No idToken — use demo card
      uCard := GetCard('000000HR');
      IF uCard IS NULL THEN
        uCard := CreateCard(null, GetType('rfid.card'), GetClient('demo'), '000000HR');
        PERFORM DoEnable(uCard);
      END IF;
    END IF;

    -- Extract meterStart from meterValue
    nMeterStart := 0;
    IF meterValue IS NOT NULL THEN
      nMeterStart := coalesce((meterValue->0->'sampledValue'->0->>'value')::integer, 0);
    END IF;

    -- Create transaction
    nTransactionId := kernel.StartTransaction(uCard, uChargePoint, nConnectorId, nMeterStart, null, tsTimestamp);

    -- Set 2.0.1-specific fields
    UPDATE db.charge_point_transaction
       SET transaction_sid = vTransactionSid, evse_id = nEvseId
     WHERE id = nTransactionId;

    -- Lock connector
    uConnector := GetConnector(uChargePoint, nConnectorId);
    IF uConnector IS NULL THEN
      uConnector := CreateConnector(uChargePoint, GetType('ac.connector'), uChargePoint, nConnectorId, GetCharger('type2.charger'), GetMode('standard.mode'));
    END IF;

    uClient := coalesce(uClient, GetCardClient(uCard));
    PERFORM kernel.StartConnectorLock(uConnector, uClient, nTransactionId);
    PERFORM kernel.CreateTransactions(nTransactionId);

    RETURN json_build_object('idTokenInfo', json_build_object('status', vStatus));

  WHEN 'Updated' THEN

    nTransactionId := ocpp.GetTransactionBySid(vTransactionSid);

    IF nTransactionId IS NOT NULL AND meterValue IS NOT NULL THEN
      PERFORM AddMeterValue(uChargePoint, nConnectorId, nTransactionId, meterValue, null,
        coalesce((meterValue->0->'sampledValue'->0->>'value')::numeric, 0), tsTimestamp);

      PERFORM kernel.UpdateTransactions(nTransactionId);

      -- Update evse_id on meter_value
      UPDATE db.meter_value SET evse_id = nEvseId
       WHERE chargepoint = uChargePoint AND transactionid = nTransactionId
         AND evse_id IS NULL;
    END IF;

    RETURN json_build_object();

  WHEN 'Ended' THEN

    nTransactionId := ocpp.GetTransactionBySid(vTransactionSid);

    IF nTransactionId IS NOT NULL THEN
      -- Extract meterStop from meterValue
      nMeterStop := 0;
      IF meterValue IS NOT NULL THEN
        nMeterStop := coalesce((meterValue->0->'sampledValue'->0->>'value')::integer, 0);
      END IF;

      PERFORM kernel.StopTransaction(nTransactionId, nMeterStop, vStoppedReason, null, tsTimestamp);
      PERFORM kernel.UpdateTransactions(nTransactionId);
    END IF;

    RETURN json_build_object();

  ELSE

    RETURN json_build_object();

  END CASE;

EXCEPTION
WHEN others THEN
  GET STACKED DIAGNOSTICS vMessage = MESSAGE_TEXT, vContext = PG_EXCEPTION_CONTEXT;

  PERFORM WriteDiagnostics(vMessage, vContext);

  RETURN json_build_object();
END;
$$ LANGUAGE plpgsql
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

**Step 3: Run update and commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db && ./runme.sh --update
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/ocpp/routine.sql
git commit -m "feat(db): add ocpp.TransactionEvent + GetTransactionBySid — OCPP 2.0.1 transaction lifecycle"
```

---

## Task 6: PG Function — ocpp.MeterValues201

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/ocpp/routine.sql`

**Context:** Standalone MeterValues in 2.0.1 uses `evseId` instead of `connectorId` + `transactionId`. Must find the active transaction for this EVSE.

**Step 1: Add function**

```sql
--------------------------------------------------------------------------------
-- ocpp.MeterValues201 --------------------------------------------------------
--------------------------------------------------------------------------------
/**
 * OCPP 2.0.1 MeterValues handler (standalone, outside TransactionEvent).
 * @param {text} pIdentity - Charge point identity
 * @param {jsonb} pRequest - {evseId, meterValue: [{timestamp, sampledValue: [{value, ...}]}]}
 * @return {json} - {} (empty per spec)
 */
CREATE OR REPLACE FUNCTION ocpp.MeterValues201 (
  pIdentity         text,
  pRequest          jsonb default null
) RETURNS           json
AS $$
DECLARE
  uChargePoint      uuid;

  arKeys            text[];

  nEvseId           integer;
  nConnectorId      integer;
  nTransactionId    integer;

  meterValue        json;
  tsTimestamp        timestamptz;

  nValue            numeric;
BEGIN
  IF pRequest IS NULL THEN
    PERFORM JsonIsEmpty();
  END IF;

  arKeys := array_cat(arKeys, ARRAY['evseId', 'meterValue']);
  PERFORM CheckJsonbKeys(pIdentity || '/MeterValues', arKeys, pRequest);

  SELECT id INTO uChargePoint FROM db.charge_point WHERE identity = pIdentity;

  IF uChargePoint IS NOT NULL THEN

    nEvseId := pRequest->>'evseId';

    meterValue := (pRequest->>'meterValue')::json;
    tsTimestamp := coalesce((meterValue->0->>'timestamp')::timestamptz, current_timestamp at time zone 'utc');
    nValue := coalesce((meterValue->0->'sampledValue'->0->>'value')::numeric, 0);

    -- Find active transaction for this EVSE
    SELECT id, connectorid INTO nTransactionId, nConnectorId
      FROM db.charge_point_transaction
     WHERE chargepoint = uChargePoint
       AND evse_id = nEvseId
       AND Now() BETWEEN datestart AND datestop
     ORDER BY datestart DESC
     LIMIT 1;

    IF nTransactionId IS NOT NULL THEN
      PERFORM AddMeterValue(uChargePoint, nConnectorId, nTransactionId, meterValue, null, nValue, tsTimestamp);

      UPDATE db.meter_value SET evse_id = nEvseId
       WHERE chargepoint = uChargePoint AND transactionid = nTransactionId
         AND evse_id IS NULL;

      PERFORM kernel.UpdateTransactions(nTransactionId);
    ELSE
      -- No active transaction — store meter value without transaction
      nConnectorId := coalesce(nEvseId, 1);
      PERFORM AddMeterValue(uChargePoint, nConnectorId, null, meterValue, null, nValue, tsTimestamp);
    END IF;
  END IF;

  RETURN json_build_object();
END;
$$ LANGUAGE plpgsql
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

**Step 2: Run update and commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db && ./runme.sh --update
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/ocpp/routine.sql
git commit -m "feat(db): add ocpp.MeterValues201 — OCPP 2.0.1 standalone meter values with evseId"
```

---

## Task 7: PG Function — ocpp.NotifyReport

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/ocpp/routine.sql`

**Context:** NotifyReport is informational — the full payload is already logged by `ocpp.WriteToLog()` in `ocpp.Parse()`. This handler just returns empty response.

**Step 1: Add function**

```sql
--------------------------------------------------------------------------------
-- ocpp.NotifyReport ----------------------------------------------------------
--------------------------------------------------------------------------------
/**
 * OCPP 2.0.1 NotifyReport handler.
 * Report data is logged via ocpp.WriteToLog in Parse(). No persistent storage needed.
 * @param {text} pIdentity - Charge point identity
 * @param {jsonb} pRequest - {requestId, generatedAt, tbc, seqNo, reportData}
 * @return {json} - {} (empty per spec)
 */
CREATE OR REPLACE FUNCTION ocpp.NotifyReport (
  pIdentity     text,
  pRequest      jsonb
) RETURNS       json
AS $$
DECLARE
  arKeys        text[];
BEGIN
  IF pRequest IS NULL THEN
    PERFORM JsonIsEmpty();
  END IF;

  arKeys := array_cat(arKeys, ARRAY['requestId', 'generatedAt', 'tbc', 'seqNo', 'reportData']);
  PERFORM CheckJsonbKeys(pIdentity || '/NotifyReport', arKeys, pRequest);

  -- Data is already logged by ocpp.WriteToLog() in ocpp.Parse()
  -- No additional processing needed

  RETURN json_build_object();
END;
$$ LANGUAGE plpgsql
   SECURITY DEFINER
   SET search_path = kernel, pg_temp;
```

**Step 2: Run update and commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db && ./runme.sh --update
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/ocpp/routine.sql
git commit -m "feat(db): add ocpp.NotifyReport — OCPP 2.0.1 device model report handler"
```

---

## Task 8: PG Function — ocpp.Parse() Version Branching

**Files:**
- Modify: `backend/db/sql/configuration/chargemecar/ocpp/routine.sql`

**Context:** Add `pVersion` parameter to existing `ocpp.Parse()` and add 2.0.1 CASE branch.

**Step 1: Update ocpp.Parse signature and add version branch**

Change the function signature (line ~1114) from:

```sql
CREATE OR REPLACE FUNCTION ocpp.Parse (
  pIdentity     text,
  pUniqueId     text,
  pAction       text,
  pPayload      jsonb,
  pAccount      text DEFAULT null
) RETURNS       json
```

To:

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

Then replace the body. After `tsBegin := clock_timestamp();` (line ~1143), change the CASE block to:

```sql
    tsBegin := clock_timestamp();

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

      CASE pAction
      WHEN 'Authorize' THEN
        jPayload := ocpp.Authorize(pIdentity, pPayload);
      -- ... (existing 1.6 CASE, unchanged)
```

Keep the rest of the function (error handling, log update, response building) exactly as-is.

**Step 2: Run update**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db && ./runme.sh --update
```

**Step 3: Test with psql**

```bash
psql "host=localhost port=5432 dbname=chargemecar user=ocpp sslmode=disable" -c "
  SELECT ocpp.parse('TEST201', 'uid-1', 'Heartbeat', '{}'::jsonb, null, '2.0.1');
"
```

Expected: JSON with `messageTypeId: CallResult`, `payload: {currentTime: ...}`

**Step 4: Commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend
git add db/sql/configuration/chargemecar/ocpp/routine.sql
git commit -m "feat(db): extend ocpp.Parse with pVersion — route 2.0.1 to new handlers"
```

---

## Task 9: C++ — Unified Routing in on_ws_message()

**Files:**
- Modify: `ocpp/src/modules/Workers/CSService/CSService.cpp` (lines 361-375)

**Step 1: Replace version-specific routing with unified chain**

Find this code block (lines 361-375):

```cpp
        if (point.ocpp_version() == "2.0.1") {
            // 2.0.1: standalone handler (PG integration will come later)
            handle_action_201(point, msg);
        } else {
#ifdef WITH_POSTGRESQL
            if (pool_) {
                parse_json_pg(point, msg);
            } else
#endif
            if (webhook_.enabled) {
                parse_json_webhook(point, msg);
            } else {
                parse_json_standalone(point, msg);
            }
        }
```

Replace with:

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

**Step 2: Verify it compiles**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/ocpp
cmake --build cmake-build-debug --parallel $(nproc) 2>&1 | tail -5
```

Expected: Build succeeds with no errors.

**Step 3: Commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/ocpp
git add src/modules/Workers/CSService/CSService.cpp
git commit -m "feat(cs): unify OCPP routing — both 1.6 and 2.0.1 go through PG when available"
```

---

## Task 10: C++ — Add Version Parameter to parse_json_pg()

**Files:**
- Modify: `ocpp/src/modules/Workers/CSService/CSService.cpp` (lines 975-981)

**Step 1: Add version parameter to SQL query**

Find this code (lines 975-981):

```cpp
    auto sql = fmt::format(
        "SELECT * FROM ocpp.parse({}, {}, {}, {}::jsonb, {})",
        pq_quote_literal(point.identity()),
        pq_quote_literal(msg.unique_id),
        pq_quote_literal(msg.action),
        pq_quote_literal(msg.payload.dump()),
        pq_quote_literal(account));
```

Replace with:

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

**Step 2: Verify it compiles**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/ocpp
cmake --build cmake-build-debug --parallel $(nproc) 2>&1 | tail -5
```

Expected: Build succeeds.

**Step 3: Commit**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/ocpp
git add src/modules/Workers/CSService/CSService.cpp
git commit -m "feat(cs): pass OCPP version to ocpp.parse() — enables 2.0.1 PG routing"
```

---

## Task 11: End-to-End Testing

**Prerequisites:** Database patched (Task 1), all PG functions deployed (Tasks 2-8), C++ rebuilt (Tasks 9-10).

**Step 1: Run the database update**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/backend/db
./runme.sh --update
```

**Step 2: Test ocpp.Parse 2.0.1 via psql**

```bash
psql "host=localhost port=5432 dbname=chargemecar user=ocpp sslmode=disable" <<'SQL'
-- Test BootNotification201
SELECT ocpp.parse('TEST-201', 'uid-boot', 'BootNotification',
  '{"chargingStation": {"vendorName": "TestVendor", "model": "TestModel", "serialNumber": "SN201"}, "reason": "PowerUp"}'::jsonb,
  null, '2.0.1');

-- Test Authorize201
SELECT ocpp.parse('TEST-201', 'uid-auth', 'Authorize',
  '{"idToken": {"idToken": "000000HR", "type": "ISO14443"}}'::jsonb,
  null, '2.0.1');

-- Test Heartbeat (shared)
SELECT ocpp.parse('TEST-201', 'uid-hb', 'Heartbeat', '{}'::jsonb, null, '2.0.1');

-- Test StatusNotification201
SELECT ocpp.parse('TEST-201', 'uid-sn', 'StatusNotification',
  '{"timestamp": "2026-03-15T12:00:00Z", "connectorStatus": "Available", "evseId": 1, "connectorId": 1}'::jsonb,
  null, '2.0.1');

-- Test NotifyReport
SELECT ocpp.parse('TEST-201', 'uid-nr', 'NotifyReport',
  '{"requestId": 1, "generatedAt": "2026-03-15T12:00:00Z", "tbc": false, "seqNo": 0}'::jsonb,
  null, '2.0.1');
SQL
```

Expected: All return `messageTypeId: CallResult` with appropriate payloads.

**Step 3: Test TransactionEvent lifecycle via psql**

```bash
psql "host=localhost port=5432 dbname=chargemecar user=ocpp sslmode=disable" <<'SQL'
-- Started
SELECT ocpp.parse('TEST-201', 'uid-tx-start', 'TransactionEvent',
  '{"eventType": "Started", "timestamp": "2026-03-15T12:00:00Z", "triggerReason": "RemoteStart", "seqNo": 0,
    "transactionInfo": {"transactionId": "test-tx-sid-001"},
    "evse": {"id": 1, "connectorId": 1},
    "idToken": {"idToken": "000000HR", "type": "ISO14443"},
    "meterValue": [{"timestamp": "2026-03-15T12:00:00Z", "sampledValue": [{"value": 100}]}]
  }'::jsonb, null, '2.0.1');

-- Verify transaction created
SELECT id, transaction_sid, evse_id, connectorid, meterstart FROM db.charge_point_transaction WHERE transaction_sid = 'test-tx-sid-001';

-- Updated
SELECT ocpp.parse('TEST-201', 'uid-tx-update', 'TransactionEvent',
  '{"eventType": "Updated", "timestamp": "2026-03-15T12:30:00Z", "triggerReason": "MeterValuePeriodic", "seqNo": 1,
    "transactionInfo": {"transactionId": "test-tx-sid-001"},
    "evse": {"id": 1, "connectorId": 1},
    "meterValue": [{"timestamp": "2026-03-15T12:30:00Z", "sampledValue": [{"value": 500}]}]
  }'::jsonb, null, '2.0.1');

-- Ended
SELECT ocpp.parse('TEST-201', 'uid-tx-end', 'TransactionEvent',
  '{"eventType": "Ended", "timestamp": "2026-03-15T13:00:00Z", "triggerReason": "RemoteStop", "seqNo": 2,
    "transactionInfo": {"transactionId": "test-tx-sid-001", "stoppedReason": "Remote"},
    "evse": {"id": 1, "connectorId": 1},
    "meterValue": [{"timestamp": "2026-03-15T13:00:00Z", "sampledValue": [{"value": 1000}]}]
  }'::jsonb, null, '2.0.1');

-- Verify final state
SELECT id, transaction_sid, evse_id, meterstart, meterstop, reason FROM db.charge_point_transaction WHERE transaction_sid = 'test-tx-sid-001';
SQL
```

Expected:
- Started: returns `{idTokenInfo: {status: "Accepted"}}`
- Transaction row: `transaction_sid = 'test-tx-sid-001'`, `evse_id = 1`, `meterstart = 100`
- Ended: `meterstop = 1000`, `reason = 'Remote'`

**Step 4: Test via Docker with emulator (if available)**

```bash
cd /home/alien/DevSrc/Projects/chargemecar/ocpp
docker compose build && docker compose up -d
# Check logs for 2.0.1 stations going through PG
docker compose logs -f cs 2>&1 | grep -E "ocpp\.parse|201"
```

**Step 5: Verify 1.6 is NOT broken**

```bash
psql "host=localhost port=5432 dbname=chargemecar user=ocpp sslmode=disable" -c "
  SELECT ocpp.parse('TEST-16', 'uid-16', 'Heartbeat', '{}'::jsonb, null, '1.6');
"
```

Expected: Same response as before — `{currentTime: ...}` with CallResult.

**Step 6: Commit test results (if any test files created)**

No source changes needed. Verify all tests pass.

---

## Task 12: Cleanup and Documentation

**Step 1: Update MEMORY.md**

Add PG integration section to `/home/alien/.claude/projects/-home-alien-DevSrc-Projects-chargemecar/memory/MEMORY.md`.

**Step 2: Push both repos**

```bash
# Backend repo
cd /home/alien/DevSrc/Projects/chargemecar/backend
git push origin master

# OCPP repo
cd /home/alien/DevSrc/Projects/chargemecar/ocpp
git push origin master
```

---

## Summary

| Task | What | Repo | Risk |
|------|------|------|------|
| 1 | Schema patch (5 ALTER TABLE) | backend | Low — IF NOT EXISTS |
| 2 | ocpp.BootNotification201 | backend | Low — new function |
| 3 | ocpp.Authorize201 | backend | Low — new function |
| 4 | ocpp.StatusNotification201 | backend | Low — new function |
| 5 | ocpp.TransactionEvent + GetTransactionBySid | backend | **Medium** — complex logic |
| 6 | ocpp.MeterValues201 | backend | Low — new function |
| 7 | ocpp.NotifyReport | backend | Low — simple handler |
| 8 | ocpp.Parse version branching | backend | **Medium** — modifies existing |
| 9 | C++ unified routing | ocpp | Low — simple reorder |
| 10 | C++ version parameter | ocpp | Low — add one parameter |
| 11 | E2E testing | both | — |
| 12 | Cleanup + push | both | — |
