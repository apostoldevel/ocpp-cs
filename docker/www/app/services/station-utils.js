/**
 * Normalize station data across OCPP 1.6 and 2.0.1 formats.
 *
 * 1.6:   bootNotification.chargePointVendor,   statusNotification.status
 * 2.0.1: bootNotification.chargingStation.vendorName, statusNotification.connectorStatus
 */

export function getVendor(station) {
  const boot = station.bootNotification || {}
  return boot.chargePointVendor
    || (boot.chargingStation || {}).vendorName
    || ''
}

export function getModel(station) {
  const boot = station.bootNotification || {}
  return boot.chargePointModel
    || (boot.chargingStation || {}).model
    || ''
}

export function getSerial(station) {
  const boot = station.bootNotification || {}
  return boot.chargePointSerialNumber
    || (boot.chargingStation || {}).serialNumber
    || ''
}

export function getFirmware(station) {
  const boot = station.bootNotification || {}
  return boot.firmwareVersion
    || (boot.chargingStation || {}).firmwareVersion
    || ''
}

export function getStatus(station) {
  const sn = station.statusNotification || {}
  return sn.status || sn.connectorStatus || 'Unknown'
}

export function getStatusClass(station) {
  return getStatus(station).toLowerCase().replace(/\s+/g, '')
}

export function isOcpp201(station) {
  return station.ocppVersion === '2.0.1'
}

export function getConnectorLabel(station) {
  const sn = station.statusNotification || {}
  if (isOcpp201(station)) {
    const evse = sn.evseId
    const conn = sn.connectorId
    if (evse != null) return `EVSE #${evse}` + (conn != null ? ` / C${conn}` : '')
    return '--'
  }
  return sn.connectorId != null ? `#${sn.connectorId}` : '--'
}

export function getErrorCode(station) {
  if (isOcpp201(station)) return ''  // 2.0.1 StatusNotification has no errorCode
  const sn = station.statusNotification || {}
  return sn.errorCode || ''
}

/** Search-friendly text for filtering */
export function searchableText(station) {
  return [
    station.identity,
    getVendor(station),
    getModel(station),
    getSerial(station),
    getStatus(station),
    station.ocppVersion,
  ].join(' ').toLowerCase()
}
