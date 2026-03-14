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

const schemaCache = {}

export async function loadSchema(commandName) {
  if (schemaCache[commandName]) return schemaCache[commandName]
  const resp = await fetch(`/app/data/schema/1.6/${commandName}.json`)
  if (!resp.ok) throw new Error(`Schema not found: ${commandName}`)
  const schema = await resp.json()
  schemaCache[commandName] = schema
  return schema
}
