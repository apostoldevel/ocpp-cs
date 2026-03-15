import { JsonView } from './json-view.js'
import { state } from '../state.js'

const { ref, computed, watch, nextTick, onMounted } = Vue

export const LogViewer = {
  components: { JsonView },
  props: {
    entries: { type: Array, required: true },
    showFilters: { type: Boolean, default: true },
    fullPage: { type: Boolean, default: false },
    maxHeight: { type: String, default: null },
    title: { type: String, default: 'Log' }
  },
  emits: ['clear'],
  setup(props, { emit }) {
    const filterIdentity = ref('')
    const filterAction = ref('')
    const filterDirection = ref('')
    const searchText = ref('')
    const autoScroll = ref(true)
    const expandedIds = ref(new Set())
    const listEl = ref(null)

    const identities = computed(() => {
      const set = new Set()
      for (const e of props.entries) if (e.identity) set.add(e.identity)
      return [...set].sort()
    })

    const actions = computed(() => {
      const set = new Set()
      for (const e of props.entries) if (e.action) set.add(e.action)
      return [...set].sort()
    })

    const filtered = computed(() => {
      let items = props.entries
      if (filterIdentity.value) items = items.filter(e => e.identity === filterIdentity.value)
      if (filterAction.value) items = items.filter(e => e.action === filterAction.value)
      if (filterDirection.value) items = items.filter(e => e.direction === filterDirection.value)
      if (searchText.value) {
        const q = searchText.value.toLowerCase()
        items = items.filter(e =>
          (e.action || '').toLowerCase().includes(q) ||
          (e.identity || '').toLowerCase().includes(q) ||
          JSON.stringify(e.payload || '').toLowerCase().includes(q)
        )
      }
      return [...items].reverse()
    })

    function togglePayload(entry) {
      const id = entry.uniqueId + entry.direction
      if (expandedIds.value.has(id)) {
        expandedIds.value.delete(id)
      } else {
        expandedIds.value.add(id)
      }
    }

    function isExpanded(entry) {
      return expandedIds.value.has(entry.uniqueId + entry.direction)
    }

    function formatTs(ts) {
      if (!ts) return ''
      return ts.replace('T', ' ').replace('Z', '').slice(11, 23)
    }

    function clearLog() {
      // Remove displayed entries from global log
      const toRemove = new Set(props.entries.map(e => e.uniqueId + e.direction + e.ts))
      for (let i = state.logEntries.length - 1; i >= 0; i--) {
        const e = state.logEntries[i]
        if (toRemove.has(e.uniqueId + e.direction + e.ts)) {
          state.logEntries.splice(i, 1)
        }
      }
      emit('clear')
    }

    function exportCsv() {
      const rows = filtered.value
      const header = 'Timestamp,Identity,Direction,MessageType,Action,Payload'
      const lines = rows.map(e => {
        const ts = (e.ts || '').replace(/"/g, '""')
        const id = (e.identity || '').replace(/"/g, '""')
        const dir = e.direction === 'in' ? 'CP→CS' : 'CS→CP'
        const type = (e.messageType || '').replace(/"/g, '""')
        const action = (e.action || '').replace(/"/g, '""')
        const payload = JSON.stringify(e.payload || {}).replace(/"/g, '""')
        return `"${ts}","${id}","${dir}","${type}","${action}","${payload}"`
      })
      const csv = header + '\n' + lines.join('\n')
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `ocpp-log-${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.csv`
      a.click()
      URL.revokeObjectURL(url)
    }

    watch(() => props.entries.length, () => {
      if (autoScroll.value && listEl.value) {
        nextTick(() => {
          listEl.value.scrollTop = 0
        })
      }
    })

    return {
      filterIdentity, filterAction, filterDirection, searchText,
      autoScroll, expandedIds, listEl, identities, actions, filtered,
      togglePayload, isExpanded, formatTs, clearLog, exportCsv
    }
  },
  template: `
    <div class="log-viewer">
      <div class="log-toolbar">
        <span style="font-size:0.82rem;color:var(--text-dim);letter-spacing:1px;text-transform:uppercase;font-weight:600">
          {{ title }}
        </span>
        <template v-if="showFilters">
          <select class="log-filter" v-model="filterIdentity">
            <option value="">All stations</option>
            <option v-for="id in identities" :key="id" :value="id">{{ id }}</option>
          </select>
          <select class="log-filter" v-model="filterAction">
            <option value="">All actions</option>
            <option v-for="a in actions" :key="a" :value="a">{{ a }}</option>
          </select>
          <select class="log-filter" v-model="filterDirection">
            <option value="">Both</option>
            <option value="in">CP &rarr; CS</option>
            <option value="out">CS &rarr; CP</option>
          </select>
          <input class="log-filter" v-model="searchText" placeholder="Search..." style="width:120px">
        </template>
        <div style="margin-left:auto;display:flex;align-items:center;gap:0.5rem">
          <button class="btn btn-sm" @click="exportCsv" title="Export to CSV">
            <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="vertical-align:-1px"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
            CSV
          </button>
          <button class="btn btn-sm" @click="clearLog" title="Clear log">
            <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="vertical-align:-1px"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>
            Clear
          </button>
          <label class="auto-scroll-toggle">
            <input type="checkbox" v-model="autoScroll"> Auto-scroll
          </label>
          <span style="font-size:0.82rem;color:var(--text-muted)">
            {{ filtered.length }} entries
          </span>
        </div>
      </div>
      <div class="log-entries" :class="{ 'full-page': fullPage }" ref="listEl"
           :style="maxHeight ? { maxHeight: maxHeight } : {}">
        <div v-if="filtered.length === 0" class="log-empty">
          Waiting for OCPP messages...
        </div>
        <template v-for="entry in filtered" :key="entry.uniqueId + entry.direction + entry.ts">
          <div class="log-entry" @click="togglePayload(entry)">
            <span class="log-ts">{{ formatTs(entry.ts) }}</span>
            <span class="log-identity">{{ entry.identity }}</span>
            <span class="log-dir" :class="entry.direction">{{ entry.direction === 'in' ? '&rarr;' : '&larr;' }}</span>
            <span class="log-type" :class="entry.messageType">{{ entry.messageType }}</span>
            <span class="log-action">{{ entry.action }}</span>
            <button class="log-payload-toggle" @click.stop="togglePayload(entry)">
              {{ isExpanded(entry) ? '[-]' : '[+]' }}
            </button>
          </div>
          <div v-if="isExpanded(entry)" class="log-payload">
            <json-view :data="entry.payload" />
          </div>
        </template>
      </div>
    </div>`
}
