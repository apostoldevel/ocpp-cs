const { ref } = Vue

export const JsonView = {
  props: {
    data: { required: true },
    collapsed: { type: Boolean, default: false }
  },
  setup(props) {
    function highlight(val, indent) {
      if (val === null) return '<span class="json-null">null</span>'
      if (typeof val === 'boolean') return `<span class="json-bool">${val}</span>`
      if (typeof val === 'number') return `<span class="json-number">${val}</span>`
      if (typeof val === 'string') {
        const escaped = val.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
        return `<span class="json-string">"${escaped}"</span>`
      }
      if (Array.isArray(val)) {
        if (val.length === 0) return '<span class="json-brace">[]</span>'
        const pad = '  '.repeat(indent)
        const inner = val.map(v => pad + '  ' + highlight(v, indent + 1)).join(',\n')
        return `<span class="json-brace">[</span>\n${inner}\n${pad}<span class="json-brace">]</span>`
      }
      if (typeof val === 'object') {
        const keys = Object.keys(val)
        if (keys.length === 0) return '<span class="json-brace">{}</span>'
        const pad = '  '.repeat(indent)
        const inner = keys.map(k => {
          const ek = k.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
          return `${pad}  <span class="json-key">"${ek}"</span>: ${highlight(val[k], indent + 1)}`
        }).join(',\n')
        return `<span class="json-brace">{</span>\n${inner}\n${pad}<span class="json-brace">}</span>`
      }
      return String(val)
    }

    function render() {
      try {
        return highlight(props.data, 0)
      } catch {
        return String(props.data)
      }
    }

    return { render }
  },
  template: `<pre class="json-view" v-html="render()"></pre>`
}
