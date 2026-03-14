import { state } from '../state.js'
import { LogViewer } from '../components/log-viewer.js'

export const LogPage = {
  components: { LogViewer },
  setup() {
    return { state }
  },
  template: `
    <div class="main-content">
      <log-viewer :entries="state.logEntries" :show-filters="true" :full-page="true" title="Live OCPP Log" />
    </div>`
}
