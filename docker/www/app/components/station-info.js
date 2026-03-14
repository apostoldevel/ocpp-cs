const { computed } = Vue

export const StationInfo = {
  props: {
    station: { type: Object, required: true }
  },
  setup(props) {
    const boot = computed(() => props.station.bootNotification || {})
    const status = computed(() => props.station.statusNotification || {})
    const statusText = computed(() => status.value.status || 'Unknown')
    const statusCls = computed(() =>
      statusText.value.toLowerCase().replace(/\s+/g, '')
    )
    return { boot, status, statusText, statusCls }
  },
  template: `
    <div class="info-panel">
      <div class="info-panel-header">
        <div style="display:flex;align-items:center;gap:0.75rem">
          <div class="card-dot" :class="statusCls" style="width:12px;height:12px"></div>
          <div class="info-panel-title">{{ station.identity }}</div>
        </div>
        <div class="card-badge" :class="statusCls">{{ statusText }}</div>
      </div>

      <div class="info-section">
        <div class="info-section-title">Boot Notification</div>
        <div class="info-row">
          <span class="info-key">Vendor</span>
          <span class="info-val">{{ boot.chargePointVendor || '--' }}</span>
        </div>
        <div class="info-row">
          <span class="info-key">Model</span>
          <span class="info-val">{{ boot.chargePointModel || '--' }}</span>
        </div>
        <div class="info-row">
          <span class="info-key">Serial Number</span>
          <span class="info-val">{{ boot.chargePointSerialNumber || '--' }}</span>
        </div>
        <div class="info-row">
          <span class="info-key">Firmware</span>
          <span class="info-val">{{ boot.firmwareVersion || '--' }}</span>
        </div>
        <div class="info-row" v-if="boot.iccid">
          <span class="info-key">ICCID</span>
          <span class="info-val">{{ boot.iccid }}</span>
        </div>
        <div class="info-row" v-if="boot.imsi">
          <span class="info-key">IMSI</span>
          <span class="info-val">{{ boot.imsi }}</span>
        </div>
        <div class="info-row" v-if="boot.meterType">
          <span class="info-key">Meter Type</span>
          <span class="info-val">{{ boot.meterType }}</span>
        </div>
        <div class="info-row" v-if="boot.meterSerialNumber">
          <span class="info-key">Meter Serial</span>
          <span class="info-val">{{ boot.meterSerialNumber }}</span>
        </div>
      </div>

      <div class="info-section">
        <div class="info-section-title">Status Notification</div>
        <div class="info-row">
          <span class="info-key">Connector</span>
          <span class="info-val">#{{ status.connectorId ?? '--' }}</span>
        </div>
        <div class="info-row">
          <span class="info-key">Status</span>
          <span class="info-val">{{ statusText }}</span>
        </div>
        <div class="info-row">
          <span class="info-key">Error Code</span>
          <span class="info-val">{{ status.errorCode || '--' }}</span>
        </div>
        <div class="info-row" v-if="status.vendorErrorCode">
          <span class="info-key">Vendor Error</span>
          <span class="info-val">{{ status.vendorErrorCode }}</span>
        </div>
        <div class="info-row" v-if="status.info">
          <span class="info-key">Info</span>
          <span class="info-val">{{ status.info }}</span>
        </div>
        <div class="info-row" v-if="status.timestamp">
          <span class="info-key">Timestamp</span>
          <span class="info-val">{{ status.timestamp }}</span>
        </div>
      </div>

      <div class="info-section">
        <div class="info-section-title">Connection</div>
        <div class="info-row">
          <span class="info-key">Protocol</span>
          <span class="info-val">{{ station.protocol || '--' }}</span>
        </div>
        <div class="info-row">
          <span class="info-key">Address</span>
          <span class="info-val">{{ station.address || '--' }}</span>
        </div>
        <div class="info-row" v-if="station.fd">
          <span class="info-key">FD</span>
          <span class="info-val">{{ station.fd }}</span>
        </div>
      </div>
    </div>`
}
