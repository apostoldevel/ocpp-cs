import { JsonView } from './json-view.js'

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
  setup(props) {
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
      return items
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

    watch(() => props.entries.length, () => {
      if (autoScroll.value && listEl.value) {
        nextTick(() => {
          listEl.value.scrollTop = listEl.value.scrollHeight
        })
      }
    })

    return {
      filterIdentity, filterAction, filterDirection, searchText,
      autoScroll, expandedIds, listEl, identities, actions, filtered,
      togglePayload, isExpanded, formatTs
    }
  },
  template: `
    <div class="log-viewer">
      <div class="log-toolbar">
        <span style="font-family:var(--mono);font-size:0.7rem;color:var(--text-dim);letter-spacing:1px;text-transform:uppercase">
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
        <label class="auto-scroll-toggle" style="margin-left:auto">
          <input type="checkbox" v-model="autoScroll"> Auto-scroll
        </label>
        <span style="font-family:var(--mono);font-size:0.65rem;color:var(--text-dim)">
          {{ filtered.length }} entries
        </span>
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
