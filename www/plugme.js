var appConfig = {
    defaultLanguage: "en",

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
    ocppApiClientId: "web-plugme.ru",

    apiTokenUrl: "http://localhost:8080/oauth2/token",
    apiDomain: "http://localhost:8080",
    wsDomain: "ws://localhost:8080",
    apiPath: "/api/v1",
    apiClientId: "web-plugme.ru",

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
