import { state } from '../state.js'
import { StationInfo } from '../components/station-info.js'
import { CommandList } from '../components/command-list.js'
import { SchemaForm } from '../components/schema-form.js'
import { CommandResponse } from '../components/command-response.js'
import { LogViewer } from '../components/log-viewer.js'
import { sendCommand } from '../services/api.js'
import { OCPP_PROFILES, loadSchema } from '../data/ocpp-commands.js'

const { ref, computed, watch } = Vue

export const StationPage = {
  components: { StationInfo, CommandList, SchemaForm, CommandResponse, LogViewer },
  props: {
    params: { type: Object, default: () => ({}) }
  },
  setup(props) {
    const identity = computed(() => props.params.id || '')
    const station = computed(() =>
      state.stations.find(s => s.identity === identity.value)
    )

    const selectedCommand = ref(null)
    const schema = ref(null)
    const response = ref(null)
    const sending = ref(false)
    const error = ref(null)

    async function onSelectCommand(cmd) {
      selectedCommand.value = cmd
      response.value = null
      error.value = null
      try {
        schema.value = await loadSchema(cmd.name)
      } catch (e) {
        schema.value = null
        error.value = 'Failed to load schema: ' + e.message
      }
    }

    async function onSubmit(payload) {
      if (!selectedCommand.value) return
      sending.value = true
      error.value = null
      response.value = null
      try {
        response.value = await sendCommand(identity.value, selectedCommand.value.name, payload)
      } catch (e) {
        error.value = e.message
      } finally {
        sending.value = false
      }
    }

    const stationLog = computed(() =>
      state.logEntries.filter(e => e.identity === identity.value)
    )

    return {
      identity, station, selectedCommand, schema, response,
      sending, error, onSelectCommand, onSubmit, stationLog
    }
  },
  template: `
    <div class="main-content">
      <div style="margin-bottom:0.75rem">
        <a href="#/" style="font-size:0.85rem;color:var(--text-muted)">&larr; All Stations</a>
      </div>

      <div v-if="!station" class="empty">
        <div class="empty-title">Station not found</div>
        <div class="empty-text">Station "{{ identity }}" is not connected.</div>
      </div>

      <template v-else>
        <!-- Station Info Bar -->
        <station-info :station="station" style="margin-bottom:0.75rem" />

        <!-- OCPP Commands (main block) -->
        <div class="info-panel" style="margin-bottom:0.75rem">
          <div class="info-panel-header">
            <div class="info-panel-title">OCPP Commands</div>
          </div>
          <div class="station-commands-panel">
            <div class="station-commands-sidebar">
              <command-list :selected="selectedCommand" @select="onSelectCommand" />
            </div>
            <div class="station-commands-main">
              <div v-if="!selectedCommand" class="station-commands-empty">
                Select a command from the list
              </div>
              <template v-else>
                <div class="station-command-title">{{ selectedCommand.name }}</div>
                <div class="station-command-desc">{{ selectedCommand.description }}</div>
                <schema-form v-if="schema" :schema="schema" :sending="sending" @submit="onSubmit" />
                <div v-if="error" style="color:var(--red);font-family:var(--mono);font-size:0.85rem;margin-top:0.5rem">
                  {{ error }}
                </div>
                <command-response v-if="response" :response="response" />
              </template>
            </div>
          </div>
        </div>

        <!-- Station Log -->
        <log-viewer :entries="stationLog" :show-filters="false" :max-height="'350px'" title="Station Log" />
      </template>
    </div>`
}
