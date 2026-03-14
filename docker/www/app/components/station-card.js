const { computed } = Vue

export const StationCard = {
  props: {
    station: { type: Object, required: true },
    index: { type: Number, default: 0 }
  },
  setup(props) {
    const boot = computed(() => props.station.bootNotification || {})
    const status = computed(() => props.station.statusNotification || {})
    const statusText = computed(() => status.value.status || 'Unknown')
    const statusCls = computed(() =>
      (statusText.value || '').toLowerCase().replace(/\s+/g, '')
    )

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

    return { boot, status, statusText, statusCls, formatTime, timeAgo, onClick }
  },
  template: `
    <div class="card" :style="{ animationDelay: index * 60 + 'ms' }" @click="onClick">
      <div class="card-header">
        <div class="card-identity">
          <div class="card-dot" :class="statusCls"></div>
          <div class="card-name">{{ station.identity }}</div>
        </div>
        <div class="card-badge" :class="statusCls">{{ statusText }}</div>
      </div>
      <div class="card-body">
        <div class="card-field">
          <div class="card-label">Vendor</div>
          <div class="card-value">{{ boot.chargePointVendor || '--' }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Model</div>
          <div class="card-value">{{ boot.chargePointModel || '--' }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Serial</div>
          <div class="card-value">{{ boot.chargePointSerialNumber || '--' }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Firmware</div>
          <div class="card-value">{{ boot.firmwareVersion || '--' }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Connector</div>
          <div class="card-value">#{{ status.connectorId ?? '--' }}</div>
        </div>
        <div class="card-field">
          <div class="card-label">Error Code</div>
          <div class="card-value">{{ status.errorCode || '--' }}</div>
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
