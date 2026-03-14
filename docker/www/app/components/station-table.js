const { ref, computed } = Vue

export const StationTable = {
  props: {
    stations: { type: Array, required: true }
  },
  setup(props) {
    const sortKey = ref('identity')
    const sortAsc = ref(true)

    const sorted = computed(() => {
      const key = sortKey.value
      const dir = sortAsc.value ? 1 : -1
      return [...props.stations].sort((a, b) => {
        let va, vb
        if (key === 'identity') {
          va = a.identity || ''
          vb = b.identity || ''
        } else if (key === 'vendor') {
          va = (a.bootNotification || {}).chargePointVendor || ''
          vb = (b.bootNotification || {}).chargePointVendor || ''
        } else if (key === 'model') {
          va = (a.bootNotification || {}).chargePointModel || ''
          vb = (b.bootNotification || {}).chargePointModel || ''
        } else if (key === 'status') {
          va = ((a.statusNotification || {}).status || '').toLowerCase()
          vb = ((b.statusNotification || {}).status || '').toLowerCase()
        } else if (key === 'protocol') {
          va = a.protocol || ''
          vb = b.protocol || ''
        } else {
          va = ''; vb = ''
        }
        return typeof va === 'string' ? dir * va.localeCompare(vb) : dir * (va - vb)
      })
    })

    function toggleSort(key) {
      if (sortKey.value === key) {
        sortAsc.value = !sortAsc.value
      } else {
        sortKey.value = key
        sortAsc.value = true
      }
    }

    function statusCls(s) {
      return ((s.statusNotification || {}).status || '').toLowerCase().replace(/\s+/g, '')
    }

    function goStation(s) {
      location.hash = '#/station/' + encodeURIComponent(s.identity)
    }

    return { sorted, sortKey, sortAsc, toggleSort, statusCls, goStation }
  },
  template: `
    <div class="info-panel">
      <table class="data-table">
        <thead>
          <tr>
            <th :class="{ sorted: sortKey === 'identity' }" @click="toggleSort('identity')">Identity</th>
            <th :class="{ sorted: sortKey === 'vendor' }" @click="toggleSort('vendor')">Vendor</th>
            <th :class="{ sorted: sortKey === 'model' }" @click="toggleSort('model')">Model</th>
            <th :class="{ sorted: sortKey === 'status' }" @click="toggleSort('status')">Status</th>
            <th :class="{ sorted: sortKey === 'protocol' }" @click="toggleSort('protocol')">Protocol</th>
            <th>Address</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="s in sorted" :key="s.identity" @click="goStation(s)">
            <td>{{ s.identity }}</td>
            <td>{{ (s.bootNotification || {}).chargePointVendor || '--' }}</td>
            <td>{{ (s.bootNotification || {}).chargePointModel || '--' }}</td>
            <td>
              <span class="card-badge" :class="statusCls(s)">
                {{ (s.statusNotification || {}).status || 'Unknown' }}
              </span>
            </td>
            <td>{{ s.protocol || '--' }}</td>
            <td>{{ s.address || '--' }}</td>
          </tr>
        </tbody>
      </table>
    </div>`
}
