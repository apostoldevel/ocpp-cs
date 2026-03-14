import { getVendor, getModel, getSerial, getFirmware, getStatus, getStatusClass, getConnectorLabel, getErrorCode, isOcpp201 } from '../services/station-utils.js'

const { computed } = Vue

export const StationInfo = {
  props: {
    station: { type: Object, required: true }
  },
  setup(props) {
    const statusText = computed(() => getStatus(props.station))
    const statusCls = computed(() => getStatusClass(props.station))
    const vendor = computed(() => getVendor(props.station))
    const model = computed(() => getModel(props.station))
    const serial = computed(() => getSerial(props.station))
    const firmware = computed(() => getFirmware(props.station))
    const connLabel = computed(() => getConnectorLabel(props.station))
    const errorCode = computed(() => getErrorCode(props.station))
    const is201 = computed(() => isOcpp201(props.station))
    const ocppVer = computed(() => props.station.ocppVersion || '1.6')

    return { statusText, statusCls, vendor, model, serial, firmware, connLabel, errorCode, is201, ocppVer }
  },
  template: `
    <div class="info-panel">
      <div class="station-info-bar">
        <div class="station-info-identity">
          <div class="card-dot" :class="statusCls" style="width:10px;height:10px"></div>
          <span class="station-info-name">{{ station.identity }}</span>
          <span class="card-badge" :class="is201 ? 'ocpp-201' : 'ocpp-16'" style="font-size:0.75rem;padding:2px 7px">{{ ocppVer }}</span>
          <div class="card-badge" :class="statusCls">{{ statusText }}</div>
        </div>
        <div class="station-info-fields">
          <div class="station-info-field" v-if="vendor">
            <span class="station-info-label">Vendor</span>
            <span class="station-info-value">{{ vendor }}</span>
          </div>
          <div class="station-info-field" v-if="model">
            <span class="station-info-label">Model</span>
            <span class="station-info-value">{{ model }}</span>
          </div>
          <div class="station-info-field" v-if="serial">
            <span class="station-info-label">Serial</span>
            <span class="station-info-value">{{ serial }}</span>
          </div>
          <div class="station-info-field" v-if="firmware">
            <span class="station-info-label">Firmware</span>
            <span class="station-info-value">{{ firmware }}</span>
          </div>
          <div class="station-info-field">
            <span class="station-info-label">{{ is201 ? 'EVSE' : 'Connector' }}</span>
            <span class="station-info-value">{{ connLabel }}</span>
          </div>
          <div class="station-info-field" v-if="!is201 && errorCode && errorCode !== 'NoError'">
            <span class="station-info-label">Error</span>
            <span class="station-info-value" style="color:var(--red)">{{ errorCode }}</span>
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
