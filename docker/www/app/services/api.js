const API_BASE = '/api/v1'

export async function getStations() {
  const resp = await fetch(`${API_BASE}/ChargePointList`)
  if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
  return resp.json()
}

export async function sendCommand(identity, command, payload = {}) {
  const resp = await fetch(`${API_BASE}/ChargePoint/${encodeURIComponent(identity)}/${command}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  })
  if (!resp.ok) {
    const text = await resp.text()
    throw new Error(text || `HTTP ${resp.status}`)
  }
  return resp.json()
}

export async function getServerTime() {
  const resp = await fetch(`${API_BASE}/time`)
  if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
  return resp.json()
}
