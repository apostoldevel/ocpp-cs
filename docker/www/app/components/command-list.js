import { OCPP_PROFILES } from '../data/ocpp-commands.js'

const { ref, computed } = Vue

const PROFILE_LABELS = {
  Core: 'Core',
  SmartCharging: 'Smart Charging',
  FirmwareManagement: 'Firmware',
  Reservation: 'Reservation',
  LocalAuthList: 'Local Auth List',
  RemoteTrigger: 'Remote Trigger',
}

export const CommandList = {
  props: {
    selected: { type: Object, default: null }
  },
  emits: ['select'],
  setup(props, { emit }) {
    const search = ref('')

    const filtered = computed(() => {
      const q = search.value.toLowerCase()
      const result = {}
      for (const [profile, cmds] of Object.entries(OCPP_PROFILES)) {
        const matching = q
          ? cmds.filter(c =>
              c.name.toLowerCase().includes(q) ||
              c.description.toLowerCase().includes(q)
            )
          : cmds
        if (matching.length > 0) result[profile] = matching
      }
      return result
    })

    function isSelected(cmd) {
      return props.selected && props.selected.name === cmd.name
    }

    return { search, filtered, PROFILE_LABELS, isSelected, emit }
  },
  template: `
    <div>
      <div style="padding:8px">
        <input class="form-input" v-model="search" placeholder="Search commands..."
               style="font-size:0.7rem;padding:5px 8px">
      </div>
      <div v-for="(cmds, profile) in filtered" :key="profile" class="command-group">
        <div class="command-group-title">{{ PROFILE_LABELS[profile] || profile }}</div>
        <button v-for="cmd in cmds" :key="cmd.name"
                class="command-item" :class="{ active: isSelected(cmd) }"
                @click="emit('select', cmd)">
          {{ cmd.name }}
          <div class="command-item-desc">{{ cmd.description }}</div>
        </button>
      </div>
    </div>`
}
