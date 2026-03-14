import { state } from '../state.js'

const MAX_LOG_ENTRIES = 500
let ws = null
let reconnectDelay = 1000

export function connectLog() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:'
  const url = `${proto}//${location.host}/ws/log`

  ws = new WebSocket(url)

  ws.onopen = () => {
    state.wsConnected = true
    reconnectDelay = 1000
  }

  ws.onmessage = (event) => {
    try {
      const entry = JSON.parse(event.data)
      state.logEntries.push(entry)
      if (state.logEntries.length > MAX_LOG_ENTRIES) {
        state.logEntries.splice(0, state.logEntries.length - MAX_LOG_ENTRIES)
      }
    } catch (e) { console.error('Log parse error:', e) }
  }

  ws.onclose = () => {
    state.wsConnected = false
    setTimeout(() => {
      reconnectDelay = Math.min(reconnectDelay * 2, 30000)
      connectLog()
    }, reconnectDelay)
  }

  ws.onerror = () => ws.close()
}

export function disconnectLog() {
  if (ws) { ws.close(); ws = null }
}
