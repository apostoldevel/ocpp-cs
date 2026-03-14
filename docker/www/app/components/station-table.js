import { getVendor, getModel, getStatus, getStatusClass } from '../services/station-utils.js'

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
          va = getVendor(a)
          vb = getVendor(b)
        } else if (key === 'model') {
          va = getModel(a)
          vb = getModel(b)
        } else if (key === 'status') {
          va = getStatus(a).toLowerCase()
          vb = getStatus(b).toLowerCase()
        } else if (key === 'ocpp') {
          va = a.ocppVersion || '1.6'
          vb = b.ocppVersion || '1.6'
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

    function goStation(s) {
      location.hash = '#/station/' + encodeURIComponent(s.identity)
    }

    function ocppCls(s) {
      const v = s.ocppVersion || '1.6'
      return v === '2.0.1' ? 'ocpp-201' : 'ocpp-16'
    }

    return { sorted, sortKey, sortAsc, toggleSort, getVendor, getModel, getStatus, getStatusClass, ocppCls, goStation }
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
            <th :class="{ sorted: sortKey === 'ocpp' }" @click="toggleSort('ocpp')">OCPP</th>
            <th :class="{ sorted: sortKey === 'protocol' }" @click="toggleSort('protocol')">Protocol</th>
            <th>Address</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="s in sorted" :key="s.identity" @click="goStation(s)">
            <td>{{ s.identity }}</td>
            <td>{{ getVendor(s) || '--' }}</td>
            <td>{{ getModel(s) || '--' }}</td>
            <td>
              <span class="card-badge" :class="getStatusClass(s)">
                {{ getStatus(s) }}
              </span>
            </td>
            <td>
              <span class="card-badge" :class="ocppCls(s)">
                {{ s.ocppVersion || '1.6' }}
              </span>
            </td>
            <td>{{ s.protocol || '--' }}</td>
            <td>{{ s.address || '--' }}</td>
          </tr>
        </tbody>
      </table>
    </div>`
}
