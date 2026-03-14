import { JsonView } from './json-view.js'

export const CommandResponse = {
  components: { JsonView },
  props: {
    response: { type: Object, default: null }
  },
  template: `
    <div v-if="response" style="margin-top:1rem">
      <div style="font-size:0.82rem;color:var(--text-dim);text-transform:uppercase;letter-spacing:1px;margin-bottom:0.5rem;font-weight:600">
        Response
      </div>
      <div style="background:var(--bg);border:1px solid var(--border);border-radius:var(--radius);padding:0.75rem;overflow-x:auto">
        <json-view :data="response" />
      </div>
    </div>`
}
