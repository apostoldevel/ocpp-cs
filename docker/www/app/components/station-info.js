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
      <div class="station-info-bar">
        <div class="station-info-identity">
          <div class="card-dot" :class="statusCls" style="width:10px;height:10px"></div>
          <span class="station-info-name">{{ station.identity }}</span>
          <div class="card-badge" :class="statusCls">{{ statusText }}</div>
        </div>
        <div class="station-info-fields">
          <div class="station-info-field" v-if="boot.chargePointVendor">
            <span class="station-info-label">Vendor</span>
            <span class="station-info-value">{{ boot.chargePointVendor }}</span>
          </div>
          <div class="station-info-field" v-if="boot.chargePointModel">
            <span class="station-info-label">Model</span>
            <span class="station-info-value">{{ boot.chargePointModel }}</span>
          </div>
          <div class="station-info-field" v-if="boot.chargePointSerialNumber">
            <span class="station-info-label">Serial</span>
            <span class="station-info-value">{{ boot.chargePointSerialNumber }}</span>
          </div>
          <div class="station-info-field" v-if="boot.firmwareVersion">
            <span class="station-info-label">Firmware</span>
            <span class="station-info-value">{{ boot.firmwareVersion }}</span>
          </div>
          <div class="station-info-field">
            <span class="station-info-label">Connector</span>
            <span class="station-info-value">#{{ status.connectorId ?? '--' }}</span>
          </div>
          <div class="station-info-field" v-if="status.errorCode && status.errorCode !== 'NoError'">
            <span class="station-info-label">Error</span>
            <span class="station-info-value" style="color:var(--red)">{{ status.errorCode }}</span>
          </div>
          <div class="station-info-field">
            <span class="station-info-label">Protocol</span>
            <span class="station-info-value">{{ station.protocol || '--' }}</span>
          </div>
          <div class="station-info-field">
            <span class="station-info-label">Address</span>
            <span class="station-info-value">{{ station.address || '--' }}</span>
          </div>
        </div>
      </div>
    </div>`
}
