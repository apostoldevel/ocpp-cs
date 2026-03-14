const { reactive } = Vue

export const state = reactive({
  stations: [],
  logEntries: [],
  wsConnected: false,
  viewMode: 'grid',
  loading: false
})
