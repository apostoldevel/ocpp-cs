import { getVendor, getModel, getSerial, getFirmware, getStatus, getStatusClass, getConnectorLabel, getErrorCode, isOcpp201 } from '../services/station-utils.js'

const { computed } = Vue

export const StationCard = {
  props: {
    station: { type: Object, required: true },
    index: { type: Number, default: 0 }
  },
  setup(props) {
    const status = computed(() => props.station.statusNotification || {})
    const statusText = computed(() => getStatus(props.station))
    const statusCls = computed(() => getStatusClass(props.station))
    const vendor = computed(() => getVendor(props.station) || '--')
    const model = computed(() => getModel(props.station) || '--')
    const serial = computed(() => getSerial(props.station) || '--')
    const firmware = computed(() => getFirmware(props.station) || '--')
    const connLabel = computed(() => getConnectorLabel(props.station))
    const errorCode = computed(() => getErrorCode(props.station))
    const is201 = computed(() => isOcpp201(props.station))
    const ocppVer = computed(() => props.station.ocppVersion || '1.6')

    function formatTime(ts) {
      if (!ts) return '--'
      try {
        return new Date(ts).toLocaleTimeString('en-GB', {
          hour: '2-digit', minute: '2-digit', second: '2-digit'
        })
      } catch { return ts }
    }

    function timeAgo(ts) {
      if (!ts) return ''
      try {
        const diff = Math.floor((Date.now() - new Date(ts).getTime()) / 1000)
        if (diff < 60)   return diff + 's ago'
        if (diff < 3600) return Math.floor(diff / 60) + 'm ago'
        return Math.floor(diff / 3600) + 'h ago'
      } catch { return '' }
    }

    function onClick() {
      location.hash = '#/station/' + encodeURIComponent(props.station.identity)
    }

    return { status, statusText, statusCls, vendor, model, serial, firmware,
             connLabel, errorCode, is201, ocppVer, formatTime, timeAgo, onClick }
  },
  template: `
    <div class="card" :style="{ animationDelay: index * 60 + 'ms' }" @click="onClick">
      <div class="card-header">
        <div class="card-identity">
          <div class="card-dot" :class="statusCls"></div>
          <div class="card-name">{{ station.identity }}</div>
        </div>
        <div style="display:flex;align-items:center;gap:6px">
          <span class="card-badge" :class="is201 ? 'ocpp-201' : 'ocpp-16'" style="font-size:0.75rem;padding:2px 7px">{{ ocppVer }}</span>
          <div class="card-badge" :class="statusCls">{{ statusText }}</div>
        </div>
      </div>
      <div class="card-body">
        <div class="card-field">
          <div class="card-label">Vendor</div>
          <div class="card-value">{{ vendor }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Model</div>
          <div class="card-value">{{ model }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Serial</div>
          <div class="card-value">{{ serial }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Firmware</div>
          <div class="card-value">{{ firmware }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">{{ is201 ? 'EVSE' : 'Connector' }}</div>
          <div class="card-value">{{ connLabel }}</div>
        </div>
        <div class="card-field" v-if="!is201">
          <div class="card-label">Error Code</div>
          <div class="card-value">{{ errorCode || '--' }}</div>
        </div>
      </div>
      <div class="card-footer">
        <div class="card-protocol">{{ station.protocol || '?' }} &middot; {{ station.address || '--' }}</div>
        <div class="card-ts">{{ formatTime(status.timestamp) }}
          <span style="opacity:0.6">{{ timeAgo(status.timestamp) }}</span>
        </div>
      </div>
    </div>`
}
