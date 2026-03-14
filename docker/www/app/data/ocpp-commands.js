export const OCPP_PROFILES = {
  Core: [
    { name: 'ChangeAvailability', description: 'Set connector online/offline' },
    { name: 'ChangeConfiguration', description: 'Change a configuration value' },
    { name: 'ClearCache', description: 'Flush authorization cache' },
    { name: 'DataTransfer', description: 'Vendor-specific data exchange' },
    { name: 'GetConfiguration', description: 'Read charge point settings' },
    { name: 'RemoteStartTransaction', description: 'Start a charging session remotely' },
    { name: 'RemoteStopTransaction', description: 'Stop an active charging session' },
    { name: 'Reset', description: 'Reboot the charge point' },
    { name: 'UnlockConnector', description: 'Unlock cable retention' },
  ],
  SmartCharging: [
    { name: 'ClearChargingProfile', description: 'Remove charging profiles' },
    { name: 'GetCompositeSchedule', description: 'Query combined charging schedule' },
    { name: 'SetChargingProfile', description: 'Set/update a charging profile' },
  ],
  FirmwareManagement: [
    { name: 'GetDiagnostics', description: 'Request diagnostic file upload' },
    { name: 'UpdateFirmware', description: 'Trigger firmware download and install' },
  ],
  Reservation: [
    { name: 'CancelReservation', description: 'Cancel a future reservation' },
    { name: 'ReserveNow', description: 'Reserve connector for a user' },
  ],
  LocalAuthList: [
    { name: 'GetLocalListVersion', description: 'Query local auth list version' },
    { name: 'SendLocalList', description: 'Push auth list to charge point' },
  ],
  RemoteTrigger: [
    { name: 'TriggerMessage', description: 'Request charge point to send a message' },
  ],
}

export const OCPP_201_PROFILES = {
  Core: [
    { name: 'RequestStartTransaction', description: 'Start a charging session remotely' },
    { name: 'RequestStopTransaction', description: 'Stop an active charging session' },
    { name: 'Reset', description: 'Reboot the station (full or per-EVSE)' },
    { name: 'ChangeAvailability', description: 'Set EVSE availability' },
    { name: 'DataTransfer', description: 'Vendor-specific data exchange' },
  ],
  DeviceModel: [
    { name: 'SetVariables', description: 'Set device model variables' },
    { name: 'GetVariables', description: 'Read device model variables' },
  ],
}

const schemaCache = {}

export async function loadSchema(commandName, version = '1.6') {
  const key = `${version}/${commandName}`
  if (schemaCache[key]) return schemaCache[key]
  const resp = await fetch(`/app/data/schema/${version}/${commandName}.json`)
  if (!resp.ok) throw new Error(`Schema not found: ${commandName} (${version})`)
  const schema = await resp.json()
  schemaCache[key] = schema
  return schema
}
