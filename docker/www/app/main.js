import { createRouter } from './router.js'
import { state } from './state.js'
import { getStations } from './services/api.js'
import { AppHeader } from './components/app-header.js'
import { DashboardPage } from './pages/dashboard.js'
import { StationPage } from './pages/station.js'
import { LogPage } from './pages/log.js'
import { connectLog } from './services/ws-log.js'
import { SchemaFormFields } from './components/schema-form.js'

const { createApp, onMounted, onUnmounted, computed } = Vue

const routes = {
  '/': 'DashboardPage',
  '/station/:id': 'StationPage',
  '/log': 'LogPage',
}

const App = {
  components: { AppHeader, DashboardPage, StationPage, LogPage },
  setup() {
    const router = createRouter(routes)

    let timer = null
    async function refresh() {
      try {
        state.stations = await getStations()
      } catch (e) { console.error('Fetch error:', e) }
    }

    onMounted(() => {
      refresh()
      timer = setInterval(refresh, 5000)
      connectLog()
    })
    onUnmounted(() => clearInterval(timer))

    return {
      currentPage: computed(() => router.current.value.page),
      routeParams: computed(() => router.current.value.params),
      state
    }
  },
  template: `
    <app-header />
    <component :is="currentPage" :params="routeParams" />`
}

const app = createApp(App)
app.component('SchemaFormFields', SchemaFormFields)
app.mount('#app')
