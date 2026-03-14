import { state } from '../state.js'

const { computed } = Vue

export const StatsBar = {
  setup() {
    const stats = computed(() => {
      const s = state.stations
      let available = 0, charging = 0, faulted = 0
      for (const p of s) {
        const st = ((p.statusNotification || {}).status || '').toLowerCase()
        if (st === 'available') available++
        else if (st === 'charging') charging++
        else if (st === 'faulted') faulted++
      }
      return { total: s.length, available, charging, faulted }
    })
    return { stats }
  },
  template: `
    <div class="stats-bar">
      <div class="stat-card">
        <div class="stat-icon total">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="7" width="20" height="14" rx="2"/><path d="M16 7V4a2 2 0 0 0-2-2h-4a2 2 0 0 0-2 2v3"/></svg>
        </div>
        <div>
          <div class="stat-value">{{ stats.total }}</div>
          <div class="stat-label">Stations</div>
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-icon online">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>
        </div>
        <div>
          <div class="stat-value">{{ stats.available }}</div>
          <div class="stat-label">Available</div>
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-icon charging">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/></svg>
        </div>
        <div>
          <div class="stat-value">{{ stats.charging }}</div>
          <div class="stat-label">Charging</div>
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-icon error">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
        </div>
        <div>
          <div class="stat-value">{{ stats.faulted }}</div>
          <div class="stat-label">Faulted</div>
        </div>
      </div>
    </div>`
}
