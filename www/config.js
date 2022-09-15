var appConfig = {
  defaultLanguage: "ru",

  creditsText: "OCPP Central System © 2022",
  creditsShortText: "OOCP CS",

  signIn: "/signin",
  signUp: "/signup",

  confAuthorize: true,
  confCrm: false,
  confOcpp: true,
  confAdmin: false,

  ocppApiDomain: "http://localhost:9220",
  ocppApiPath: "/api/v1",
  ocppApiClienId: "web-ocpp-css.com",

  apiTokenUrl: "/oauth2/token",
  apiDomain: "http://localhost:8080",
  wsDomain: "ws://localhost:9220",
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

  maxFileSize: 512000,
};
