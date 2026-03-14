import { state } from '../state.js'
import { StatsBar } from '../components/stats-bar.js'
import { StationCard } from '../components/station-card.js'
import { StationTable } from '../components/station-table.js'

const { computed, ref, onMounted, onUnmounted } = Vue

export const DashboardPage = {
  components: { StatsBar, StationCard, StationTable },
  setup() {
    const search = ref('')

    const sorted = computed(() => {
      let list = [...state.stations].sort((a, b) =>
        (a.identity || '').localeCompare(b.identity || '')
      )
      const q = search.value.trim().toLowerCase()
      if (q) {
        list = list.filter(s => {
          const id = (s.identity || '').toLowerCase()
          const vendor = ((s.bootNotification || {}).chargePointVendor || '').toLowerCase()
          const model = ((s.bootNotification || {}).chargePointModel || '').toLowerCase()
          const serial = ((s.bootNotification || {}).chargePointSerialNumber || '').toLowerCase()
          const status = (((s.statusNotification || {}).status) || '').toLowerCase()
          return id.includes(q) || vendor.includes(q) || model.includes(q) || serial.includes(q) || status.includes(q)
        })
      }
      return list
    })

    const refreshPct = ref(0)
    let rafId = null
    let refreshStart = Date.now()

    function animateBar() {
      const elapsed = Date.now() - refreshStart
      refreshPct.value = Math.min((elapsed / 5000) * 100, 100)
      if (refreshPct.value >= 100) refreshStart = Date.now()
      rafId = requestAnimationFrame(animateBar)
    }

    onMounted(() => { rafId = requestAnimationFrame(animateBar) })
    onUnmounted(() => cancelAnimationFrame(rafId))

    return { state, sorted, search, refreshPct }
  },
  template: `
    <stats-bar />
    <div class="main-content">
      <div class="section-header">
        <div class="section-title">Charge Points</div>
        <div class="section-actions">
          <input
            v-model="search"
            type="text"
            class="search-input"
            placeholder="Search stations..."
            style="width: 240px"
          />
          <div class="view-toggle">
            <button :class="{ active: state.viewMode === 'grid' }" @click="state.viewMode = 'grid'" title="Grid view">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg>
            </button>
            <button :class="{ active: state.viewMode === 'table' }" @click="state.viewMode = 'table'" title="Table view">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/></svg>
            </button>
          </div>
          <div class="refresh-info">
            <span>auto-refresh 5s</span>
            <div class="refresh-bar">
              <div class="refresh-bar-fill" :style="{ width: refreshPct + '%' }"></div>
            </div>
          </div>
        </div>
      </div>

      <div v-if="state.stations.length === 0" class="empty">
        <div class="empty-icon">&#9889;</div>
        <div class="empty-title">No stations connected</div>
        <div class="empty-text">Waiting for charge points to connect via WebSocket on <code>/ocpp/{identity}</code></div>
      </div>

      <div v-else-if="sorted.length === 0" class="empty">
        <div class="empty-icon">&#128269;</div>
        <div class="empty-title">No matches</div>
        <div class="empty-text">No stations match "{{ search }}"</div>
      </div>

      <div v-else-if="state.viewMode === 'grid'" class="station-grid">
        <station-card v-for="(s, i) in sorted" :key="s.identity" :station="s" :index="i" />
      </div>

      <station-table v-else :stations="sorted" />
    </div>

    <footer class="footer">
      OCPP Central System &middot;
      Built on <a href="https://github.com/apostoldevel/libapostol" target="_blank">A-POST-OL</a> framework &middot;
      <a href="https://github.com/apostoldevel/ocpp-cs" target="_blank">GitHub</a>
    </footer>`
}
