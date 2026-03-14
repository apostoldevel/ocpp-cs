import { state } from '../state.js'

const { ref, onMounted, onUnmounted } = Vue

export const AppHeader = {
  setup() {
    const clock = ref('')

    let timer = null
    function updateClock() {
      const now = new Date()
      clock.value = now.toISOString().replace('T', '  ').slice(0, 21) + ' UTC'
    }

    onMounted(() => { updateClock(); timer = setInterval(updateClock, 1000) })
    onUnmounted(() => clearInterval(timer))

    const currentHash = ref(location.hash || '#/')
    function onHashChange() { currentHash.value = location.hash || '#/' }
    onMounted(() => window.addEventListener('hashchange', onHashChange))
    onUnmounted(() => window.removeEventListener('hashchange', onHashChange))

    function isActive(path) {
      const h = currentHash.value.slice(1) || '/'
      if (path === '/') return h === '/'
      return h.startsWith(path)
    }

    return { state, clock, isActive }
  },
  template: `
    <header class="header">
     <div class="header-inner">
      <div class="header-left">
        <a href="#/" style="display:flex;align-items:center;gap:0.75rem;text-decoration:none">
          <div class="logo-mark">CS</div>
          <div class="logo-text">OCPP Central System <span>v2.0</span></div>
        </a>
        <nav class="header-nav">
          <a href="#/" :class="{ active: isActive('/') }">Stations</a>
          <a href="#/log" :class="{ active: isActive('/log') }">Log</a>
        </nav>
      </div>
      <div class="header-right">
        <div class="server-time">{{ clock }}</div>
        <div class="ws-status">
          <div class="pulse-dot" :class="{ disconnected: !state.wsConnected }"></div>
          <span class="status-label" :class="{ disconnected: !state.wsConnected }">
            {{ state.wsConnected ? 'LIVE' : 'OFFLINE' }}
          </span>
        </div>
        <a href="/docs/" class="nav-link-btn" target="_blank">API Docs</a>
      </div>
     </div>
    </header>`
}
