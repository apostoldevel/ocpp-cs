const { ref, watch, computed } = Vue

export const SchemaForm = {
  props: {
    schema: { type: Object, required: true },
    sending: { type: Boolean, default: false }
  },
  emits: ['submit'],
  setup(props, { emit }) {
    const formData = ref({})

    watch(() => props.schema, () => {
      formData.value = buildDefault(props.schema)
    }, { immediate: true })

    function buildDefault(schema) {
      if (!schema || !schema.properties) return {}
      const obj = {}
      for (const [key, prop] of Object.entries(schema.properties)) {
        if (prop.type === 'object' && prop.properties) {
          obj[key] = buildDefault(prop)
        } else if (prop.type === 'array') {
          obj[key] = []
        } else if (prop.type === 'integer' || prop.type === 'number') {
          obj[key] = null
        } else if (prop.type === 'boolean') {
          obj[key] = null
        } else {
          obj[key] = ''
        }
      }
      return obj
    }

    function onSubmit() {
      const payload = cleanPayload(formData.value, props.schema)
      emit('submit', payload)
    }

    function cleanPayload(data, schema) {
      if (!schema || !schema.properties) return data
      const result = {}
      const required = schema.required || []
      for (const [key, prop] of Object.entries(schema.properties)) {
        const val = data[key]
        if (prop.type === 'object' && prop.properties) {
          const nested = cleanPayload(val || {}, prop)
          if (Object.keys(nested).length > 0 || required.includes(key)) {
            result[key] = nested
          }
        } else if (prop.type === 'array') {
          if (Array.isArray(val) && val.length > 0) {
            if (prop.items && prop.items.type === 'object') {
              result[key] = val.map(item => cleanPayload(item, prop.items))
            } else {
              result[key] = val.filter(v => v !== '' && v != null)
            }
          } else if (required.includes(key)) {
            result[key] = []
          }
        } else if (prop.type === 'integer' || prop.type === 'number') {
          if (val !== null && val !== '' && !isNaN(val)) {
            result[key] = prop.type === 'integer' ? parseInt(val) : parseFloat(val)
          } else if (required.includes(key)) {
            result[key] = 0
          }
        } else if (prop.type === 'boolean') {
          if (val !== null && val !== '') {
            result[key] = val === true || val === 'true'
          }
        } else {
          if (val !== '' && val != null) {
            result[key] = String(val)
          } else if (required.includes(key)) {
            result[key] = ''
          }
        }
      }
      return result
    }

    const properties = computed(() => {
      if (!props.schema || !props.schema.properties) return []
      const required = props.schema.required || []
      return Object.entries(props.schema.properties).map(([key, prop]) => ({
        key,
        prop,
        required: required.includes(key)
      }))
    })

    function addArrayItem(key, itemSchema) {
      if (!formData.value[key]) formData.value[key] = []
      if (itemSchema && itemSchema.type === 'object') {
        formData.value[key].push(buildDefault(itemSchema))
      } else {
        formData.value[key].push('')
      }
    }

    function removeArrayItem(key, index) {
      formData.value[key].splice(index, 1)
    }

    return { formData, properties, onSubmit, addArrayItem, removeArrayItem, buildDefault }
  },
  template: `
    <form @submit.prevent="onSubmit">
      <template v-for="{ key, prop, required } in properties" :key="key">
        <!-- String with enum -->
        <div v-if="prop.type === 'string' && prop.enum" class="form-group">
          <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
          <select class="form-select" v-model="formData[key]">
            <option value="">-- select --</option>
            <option v-for="opt in prop.enum" :key="opt" :value="opt">{{ opt }}</option>
          </select>
        </div>

        <!-- String with date-time -->
        <div v-else-if="prop.type === 'string' && prop.format === 'date-time'" class="form-group">
          <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
          <input class="form-input" type="datetime-local" v-model="formData[key]"
                 step="1" :maxlength="prop.maxLength">
        </div>

        <!-- String -->
        <div v-else-if="prop.type === 'string'" class="form-group">
          <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
          <input class="form-input" type="text" v-model="formData[key]"
                 :maxlength="prop.maxLength" :placeholder="prop.maxLength ? 'max ' + prop.maxLength + ' chars' : ''">
        </div>

        <!-- Integer -->
        <div v-else-if="prop.type === 'integer'" class="form-group">
          <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
          <input class="form-input" type="number" step="1" v-model="formData[key]">
        </div>

        <!-- Number -->
        <div v-else-if="prop.type === 'number'" class="form-group">
          <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
          <input class="form-input" type="number" step="0.1" v-model="formData[key]">
        </div>

        <!-- Boolean -->
        <div v-else-if="prop.type === 'boolean'" class="form-group">
          <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
          <select class="form-select" v-model="formData[key]">
            <option value="">-- select --</option>
            <option :value="true">true</option>
            <option :value="false">false</option>
          </select>
        </div>

        <!-- Nested object -->
        <fieldset v-else-if="prop.type === 'object' && prop.properties" class="form-fieldset">
          <legend class="form-fieldset-legend">{{ key }}<span v-if="required" class="required">*</span></legend>
          <schema-form-fields :schema="prop" :model="formData[key]" />
        </fieldset>

        <!-- Array of strings -->
        <div v-else-if="prop.type === 'array' && (!prop.items || prop.items.type === 'string')" class="form-group">
          <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
          <textarea class="form-textarea" :value="(formData[key] || []).join('\\n')"
                    @input="formData[key] = $event.target.value.split('\\n').filter(v => v)"
                    placeholder="One value per line"></textarea>
        </div>

        <!-- Array of objects -->
        <fieldset v-else-if="prop.type === 'array' && prop.items && prop.items.type === 'object'" class="form-fieldset">
          <legend class="form-fieldset-legend">{{ key }}<span v-if="required" class="required">*</span></legend>
          <div v-for="(item, idx) in (formData[key] || [])" :key="idx"
               style="border:1px solid var(--border);border-radius:var(--radius);padding:0.5rem;margin-bottom:0.5rem;position:relative">
            <button type="button" class="btn btn-sm" @click="removeArrayItem(key, idx)"
                    style="position:absolute;top:4px;right:4px;font-size:0.82rem;padding:2px 8px">x</button>
            <schema-form-fields :schema="prop.items" :model="item" />
          </div>
          <button type="button" class="btn btn-sm" @click="addArrayItem(key, prop.items)">+ Add {{ key }}</button>
        </fieldset>
      </template>

      <button type="submit" class="btn btn-primary" :disabled="sending" style="margin-top:0.75rem">
        {{ sending ? 'Sending...' : 'Send Command' }}
      </button>
    </form>`
}

// Recursive sub-component for nested objects
export const SchemaFormFields = {
  name: 'SchemaFormFields',
  props: {
    schema: { type: Object, required: true },
    model: { type: Object, required: true }
  },
  setup(props) {
    const properties = computed(() => {
      if (!props.schema || !props.schema.properties) return []
      const required = props.schema.required || []
      return Object.entries(props.schema.properties).map(([key, prop]) => ({
        key, prop, required: required.includes(key)
      }))
    })

    function initKey(key, prop) {
      if (props.model[key] === undefined) {
        if (prop.type === 'object' && prop.properties) {
          props.model[key] = {}
          for (const [k, p] of Object.entries(prop.properties)) {
            initKey.call(null, k, p)
          }
        } else if (prop.type === 'array') {
          props.model[key] = []
        } else if (prop.type === 'integer' || prop.type === 'number') {
          props.model[key] = null
        } else {
          props.model[key] = ''
        }
      }
    }

    // ensure model keys exist
    for (const { key, prop } of properties.value) {
      initKey(key, prop)
    }

    function addItem(key, itemSchema) {
      if (!props.model[key]) props.model[key] = []
      if (itemSchema && itemSchema.type === 'object') {
        const obj = {}
        if (itemSchema.properties) {
          for (const [k, p] of Object.entries(itemSchema.properties)) {
            if (p.type === 'integer' || p.type === 'number') obj[k] = null
            else obj[k] = ''
          }
        }
        props.model[key].push(obj)
      } else {
        props.model[key].push('')
      }
    }

    function removeItem(key, idx) {
      props.model[key].splice(idx, 1)
    }

    return { properties, addItem, removeItem }
  },
  template: `
    <template v-for="{ key, prop, required } in properties" :key="key">
      <div v-if="prop.type === 'string' && prop.enum" class="form-group">
        <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
        <select class="form-select" v-model="model[key]">
          <option value="">-- select --</option>
          <option v-for="opt in prop.enum" :key="opt" :value="opt">{{ opt }}</option>
        </select>
      </div>
      <div v-else-if="prop.type === 'string' && prop.format === 'date-time'" class="form-group">
        <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
        <input class="form-input" type="datetime-local" v-model="model[key]" step="1">
      </div>
      <div v-else-if="prop.type === 'string'" class="form-group">
        <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
        <input class="form-input" type="text" v-model="model[key]" :maxlength="prop.maxLength">
      </div>
      <div v-else-if="prop.type === 'integer'" class="form-group">
        <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
        <input class="form-input" type="number" step="1" v-model="model[key]">
      </div>
      <div v-else-if="prop.type === 'number'" class="form-group">
        <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
        <input class="form-input" type="number" step="0.1" v-model="model[key]">
      </div>
      <div v-else-if="prop.type === 'boolean'" class="form-group">
        <label class="form-label">{{ key }}<span v-if="required" class="required">*</span></label>
        <select class="form-select" v-model="model[key]">
          <option value="">-- select --</option>
          <option :value="true">true</option>
          <option :value="false">false</option>
        </select>
      </div>
      <fieldset v-else-if="prop.type === 'object' && prop.properties" class="form-fieldset">
        <legend class="form-fieldset-legend">{{ key }}</legend>
        <schema-form-fields :schema="prop" :model="model[key]" />
      </fieldset>
      <div v-else-if="prop.type === 'array' && (!prop.items || prop.items.type === 'string')" class="form-group">
        <label class="form-label">{{ key }}</label>
        <textarea class="form-textarea" :value="(model[key] || []).join('\\n')"
                  @input="model[key] = $event.target.value.split('\\n').filter(v => v)"
                  placeholder="One value per line"></textarea>
      </div>
      <fieldset v-else-if="prop.type === 'array' && prop.items && prop.items.type === 'object'" class="form-fieldset">
        <legend class="form-fieldset-legend">{{ key }}</legend>
        <div v-for="(item, idx) in (model[key] || [])" :key="idx"
             style="border:1px solid var(--border);border-radius:var(--radius);padding:0.5rem;margin-bottom:0.5rem;position:relative">
          <button type="button" class="btn btn-sm" @click="removeItem(key, idx)"
                  style="position:absolute;top:4px;right:4px;font-size:0.82rem;padding:2px 8px">x</button>
          <schema-form-fields :schema="prop.items" :model="item" />
        </div>
        <button type="button" class="btn btn-sm" @click="addItem(key, prop.items)">+ Add</button>
      </fieldset>
    </template>`
}
