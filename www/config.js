var appConfig = {
  defaultLanguage: "en",

  creditsText: "OCPP Central System © 2024",
  creditsShortText: "Central System",

  signIn: "/signin",
  signUp: "/signup",

  confAuthorize: true,
  confCrm: false,
  confDriver: false,
  confOcpp: true,
  confAdmin: false,
  googleAuthorize: false,

  mapLanguage: "en_US",

  googleClientId: null,

  ocppApiDomain: "http://localhost:9220",
  ocppApiPath: "/api/v1",
  ocppApiClientId: "web-ocpp-css.com",

  apiTokenUrl: "http://localhost:8080/oauth2/token",
  apiDomain: "http://localhost:8080",
  wsDomain: "ws://localhost:8080",
  apiPath: "/api/v1",
  apiClientId: "web-ocpp-css.com",

  adminReferences: {
    model: {
      extraFields: {
        vendor: {
          type: "reference",
          path: "/vendor/list",
          required: true,
        },
      },
    },
    vendor: {},
    network: {},
  },

  maxFileSize: 512000
};
