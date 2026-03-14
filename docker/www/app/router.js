const { ref, readonly } = Vue

export function createRouter(routes) {
  const current = ref({ page: null, params: {} })

  function resolve() {
    const hash = location.hash.slice(1) || '/'
    for (const [pattern, page] of Object.entries(routes)) {
      const params = matchRoute(pattern, hash)
      if (params !== null) {
        current.value = { page, params }
        return
      }
    }
    const [, page] = Object.entries(routes)[0]
    current.value = { page, params: {} }
  }

  function matchRoute(pattern, path) {
    const pp = pattern.split('/').filter(Boolean)
    const hp = path.split('/').filter(Boolean)
    if (pp.length !== hp.length) return null
    const params = {}
    for (let i = 0; i < pp.length; i++) {
      if (pp[i].startsWith(':')) {
        params[pp[i].slice(1)] = decodeURIComponent(hp[i])
      } else if (pp[i] !== hp[i]) {
        return null
      }
    }
    return params
  }

  window.addEventListener('hashchange', resolve)
  resolve()

  return {
    current: readonly(current),
    navigate(hash) { location.hash = hash }
  }
}
