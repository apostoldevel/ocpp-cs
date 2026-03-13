#include "CSService.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#ifdef WITH_POSTGRESQL
#include "apostol/pg_exec.hpp"
#endif

#include "apostol/base64.hpp"
#include "apostol/fetch_client.hpp"

#include <chrono>
#include <filesystem>

namespace apostol
{

using json = nlohmann::json;

// ── Helpers ─────────────────────────────────────────────────────────────────

namespace
{

// Split path into components: "/a/b/c" -> ["a", "b", "c"]
std::vector<std::string_view> split_path(std::string_view path)
{
    std::vector<std::string_view> parts;
    if (path.starts_with('/'))
        path.remove_prefix(1);
    while (!path.empty()) {
        auto pos = path.find('/');
        parts.push_back(path.substr(0, pos));
        if (pos == std::string_view::npos) break;
        path.remove_prefix(pos + 1);
    }
    return parts;
}

} // namespace

// ── Constructor / Destructor ─────────────────────────────────────────────────

CSService::~CSService() = default;

CSService::CSService(Application& app)
    : app_(app)
#ifdef WITH_POSTGRESQL
    , pool_(app.db_pool())
#endif
    , enabled_(app.module_enabled("CSService"))
{
    // Load webhook configuration from JSON config
    const auto& cfg = app.config().json();
    if (cfg.contains("webhook")) {
        const auto& wh = cfg["webhook"];
        webhook_.enabled     = wh.value("enable", false);
        webhook_.url         = wh.value("url", "");
        webhook_.auth_scheme = wh.value("authorization", "Off");
        webhook_.username    = wh.value("username", "");
        webhook_.password    = wh.value("password", "");
        webhook_.token       = wh.value("token", "");
    }

    // Environment variable overrides
    if (const char* env_url = std::getenv("WEBHOOK_URL")) {
        webhook_.url = env_url;
        webhook_.enabled = true;
    }

    // Register WS handler
    app_.set_ws_handler([this](EventLoop& loop, WsConnection ws, const HttpRequest& req) {
        on_ws_upgrade(loop, std::move(ws), req);
    });
}

// ── check_location ──────────────────────────────────────────────────────────

bool CSService::check_location(const HttpRequest& req) const
{
    const auto& path = req.path;
    return path.starts_with("/ocpp/")
        || path.starts_with("/Ocpp/")
        || path.starts_with("/api/");
}

// ── init_methods ────────────────────────────────────────────────────────────

void CSService::init_methods()
{
    add_method("GET",  [this](auto& req, auto& resp) { do_get(req, resp); });
    add_method("POST", [this](auto& req, auto& resp) { do_post(req, resp); });
}

// ── HTTP GET ────────────────────────────────────────────────────────────────

void CSService::do_get(const HttpRequest& req, HttpResponse& resp)
{
    const auto& path = req.path;

    if (path.starts_with("/api/")) {
        do_api(req, resp);
        return;
    }

    // WebSocket upgrade is handled via set_ws_handler, not here.
    // GET /ocpp/* without upgrade -> 400
    if (path.starts_with("/ocpp/")) {
        reply_error(resp, HttpStatus::bad_request, "WebSocket upgrade required");
        return;
    }

    // Serve static files from www/
    auto file_path = app_.settings().doc_root / path.substr(1);
    if (!serve_file(file_path, resp, false)) {
        auto index = app_.settings().doc_root / "index.html";
        if (!serve_file(index, resp, false)) {
            reply_error(resp, HttpStatus::not_found, "Not Found");
        }
    }
}

// ── HTTP POST ───────────────────────────────────────────────────────────────

void CSService::do_post(const HttpRequest& req, HttpResponse& resp)
{
    const auto parts = split_path(req.path);
    if (parts.empty()) {
        reply_error(resp, HttpStatus::bad_request, "Invalid path");
        return;
    }

    if (parts[0] == "api") {
        do_api(req, resp);
        return;
    }

#ifdef WITH_POSTGRESQL
    if (parts[0] == "Ocpp") {
        do_soap(req, resp);
        return;
    }
#endif

    reply_error(resp, HttpStatus::not_found, "Not Found");
}

// ── WebSocket Upgrade (OCPP 1.6 JSON) ───────────────────────────────────────

void CSService::on_ws_upgrade(EventLoop& loop, WsConnection ws, const HttpRequest& req)
{
    const auto parts = split_path(req.path);

    // Extract identity from path: /ocpp/{identity} or /ocpp/{account}/{identity}
    if (parts.size() < 2 || parts[0] != "ocpp") {
        ws.send_close(1002, "Invalid path");
        return;
    }

    std::string identity;
    std::string account;

    if (parts.size() >= 3) {
        account  = std::string(parts[1]);
        identity = std::string(parts[2]);
    } else {
        identity = std::string(parts[1]);
    }

    auto& point = point_manager_.get_or_create(identity);
    point.set_protocol_type(ocpp::ProtocolType::JSON);
    point.set_address(get_host(req));
    point.set_connected_at(ocpp::CSChargingPoint::clock::now());
    point.touch();

    int fd = ws.fd();

    // Store WsConnection
    auto [it, inserted] = ws_connections_.emplace(fd, std::move(ws));
    auto* ws_ptr = &it->second;

    point.set_ws_connection(ws_ptr);

    app_.logger().notice("[{}] connected (fd={}, address={})", identity, fd, point.address());

#ifdef WITH_POSTGRESQL
    // Notify database
    set_point_connected(identity, true, charge_point_to_json(point));
#endif

    // Register read handler
    loop.add_io(fd, EPOLLIN, [this, fd, identity_copy = identity](uint32_t /*events*/) {
        auto ws_it = ws_connections_.find(fd);
        if (ws_it == ws_connections_.end()) return;

        auto& ws_conn = ws_it->second;
        bool alive = ws_conn.on_readable(
            [this, &identity_copy](uint8_t opcode, const std::string& payload) {
                if (opcode == WS_OP_TEXT) {
                    auto* point = point_manager_.find_by_identity(identity_copy);
                    if (point) {
                        on_ws_message(*point, payload);
                    }
                }
            },
            [this, identity_copy]() {
                on_ws_close(identity_copy);
            }
        );

        if (!alive) {
            on_ws_close(identity_copy);
        }
    });
}

void CSService::on_ws_message(ocpp::CSChargingPoint& point, const std::string& payload)
{
    point.touch();

    ocpp::OcppMessage msg;
    try {
        msg = ocpp::parse_ocpp_json(payload);
    } catch (const std::exception& e) {
        app_.logger().error("[{}] JSON parse error: {}", point.identity(), e.what());
        return;
    }

    log_json_message(point.identity(), msg);

    if (msg.type == ocpp::MessageType::Call) {
        // Store last request
        point.store_request(msg.action, msg.payload);

#ifdef WITH_POSTGRESQL
        parse_json_pg(point, msg);
#else
        if (webhook_.enabled) {
            parse_json_webhook(point, msg);
        } else {
            parse_json_standalone(point, msg);
        }
#endif
    }
    // CallResult/CallError responses are handled by WsClient correlation
    // (for outbound requests). Inbound responses to our calls are TODO.
}

void CSService::on_ws_close(const std::string& identity)
{
    auto* point = point_manager_.find_by_identity(identity);
    if (!point) return;

    int fd = -1;
    if (point->ws_connection()) {
        fd = point->ws_connection()->fd();
    }

    app_.logger().notice("[{}] disconnected (fd={})", identity, fd);

#ifdef WITH_POSTGRESQL
    set_point_connected(identity, false, json::object());
#endif

    point->set_ws_connection(nullptr);

    if (fd >= 0) {
        app_.worker_loop().remove_io(fd);
        ws_connections_.erase(fd);
    }
}

void CSService::send_json_response(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& response)
{
    log_json_message(point.identity(), response);
    point.send_json(response);
}

// ── SOAP (OCPP 1.5) — WITH_POSTGRESQL only ──────────────────────────────────

#ifdef WITH_POSTGRESQL

void CSService::do_soap(const HttpRequest& req, HttpResponse& resp)
{
    if (req.body.empty()) {
        reply_error(resp, HttpStatus::bad_request, "Empty SOAP body");
        return;
    }

    parse_soap(req, resp, req.body);
}

void CSService::parse_soap(const HttpRequest& req, HttpResponse& resp, const std::string& payload)
{
    auto sql = fmt::format("SELECT * FROM ocpp.\"ParseXML\"({}::xml)",
        pq_quote_literal(payload));

    exec_sql(pool_, req, resp, std::move(sql),
        [this](std::shared_ptr<HttpConnection> conn, std::vector<PgResult> results) {
            if (results.empty() || !results[0].ok()) {
                HttpResponse r;
                reply_error(r, HttpStatus::internal_server_error, "Database error");
                conn->send_response(r);
                return;
            }

            const auto& result = results[0];
            if (result.rows() == 0) {
                HttpResponse r;
                reply_error(r, HttpStatus::internal_server_error, "Empty result");
                conn->send_response(r);
                return;
            }

            int col_identity = result.column_index("identity");
            int col_address  = result.column_index("address");
            int col_payload  = result.column_index("payload");

            const char* identity = result.value(0, col_identity);
            const char* address  = result.value(0, col_address);
            const char* xml_resp = result.value(0, col_payload);

            // Update point manager
            auto& point = point_manager_.get_or_create(identity);
            point.set_protocol_type(ocpp::ProtocolType::SOAP);
            if (address && address[0] != '\0')
                point.set_address(address);
            point.touch();

            set_point_connected(identity, true, charge_point_to_json(point));

            // Send SOAP response
            HttpResponse r;
            r.set_status(HttpStatus::ok);
            r.set_body(xml_resp ? xml_resp : "", "application/soap+xml; charset=utf-8");
            conn->send_response(r);
        });
}

void CSService::json_to_soap(HttpResponse& resp, ocpp::CSChargingPoint* point,
                             const std::string& operation, const nlohmann::json& payload)
{
    json data = {
        {"header", {
            {"chargeBoxIdentity", point->identity()},
            {"Action", operation},
            {"To", point->address()}
        }},
        {"payload", payload}
    };

    auto sql = fmt::format("SELECT * FROM ocpp.\"JSONToSOAP\"({}::jsonb)",
        pq_quote_literal(data.dump()));

    // TODO: deferred response for REST API -> SOAP conversion
    (void)resp;
    (void)sql;
}

void CSService::soap_to_json(HttpResponse& resp, const std::string& xml_payload)
{
    (void)resp;
    (void)xml_payload;
    // TODO: SELECT * FROM ocpp."SOAPToJSON"(...)
}

#endif // WITH_POSTGRESQL

// ── REST API ────────────────────────────────────────────────────────────────

void CSService::do_api(const HttpRequest& req, HttpResponse& resp)
{
    const auto parts = split_path(req.path);

    // /api/v1/{command}
    if (parts.size() < 3 || parts[0] != "api" || parts[1] != "v1") {
        reply_error(resp, HttpStatus::bad_request, "Invalid API path");
        return;
    }

    const auto& command = parts[2];

    if (command == "ping") {
        resp.set_status(HttpStatus::ok);
        resp.set_body("OK", "text/plain");
        return;
    }

    if (command == "time") {
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        json result = {{"serverTime", now_ms}};
        resp.set_status(HttpStatus::ok);
        resp.set_body(result.dump(), "application/json");
        return;
    }

    if (command == "ChargePointList") {
        do_charge_point_list(req, resp);
        return;
    }

#ifdef WITH_POSTGRESQL
    if (command == "CentralSystem" && parts.size() >= 4) {
        do_central_system(req, resp, std::string(parts[3]));
        return;
    }

    if (command == "ChargePoint" && parts.size() >= 5) {
        do_charge_point(req, resp, std::string(parts[3]), std::string(parts[4]));
        return;
    }
#endif

    reply_error(resp, HttpStatus::not_found, "Unknown API command");
}

void CSService::do_charge_point_list(const HttpRequest& /*req*/, HttpResponse& resp)
{
    auto list = get_charge_point_list();
    resp.set_status(HttpStatus::ok);
    resp.set_body(list.dump(), "application/json");
}

#ifdef WITH_POSTGRESQL

void CSService::do_central_system(const HttpRequest& req, HttpResponse& resp,
                                  const std::string& endpoint)
{
    auto body = content_to_json(req);

    auto sql = fmt::format("SELECT * FROM ocpp.\"{}\"({}::jsonb)",
        endpoint, pq_quote_literal(body.dump()));

    exec_sql(pool_, req, resp, std::move(sql),
        [](std::shared_ptr<HttpConnection> conn, std::vector<PgResult> results) {
            HttpResponse r;
            ApostolModule::reply_pg(r, results);
            conn->send_response(r);
        });
}

void CSService::do_charge_point(const HttpRequest& req, HttpResponse& resp,
                               const std::string& identity, const std::string& operation)
{
    auto* point = point_manager_.find_by_identity(identity);
    if (!point) {
        reply_error(resp, HttpStatus::not_found,
            fmt::format("Charge point '{}' not found", identity));
        return;
    }

    if (!point->connected()) {
        reply_error(resp, HttpStatus::service_unavailable,
            fmt::format("Charge point '{}' is not connected", identity));
        return;
    }

    auto body = content_to_json(req);

    if (point->protocol_type() == ocpp::ProtocolType::JSON) {
        // Send JSON via WebSocket
        auto msg = ocpp::make_call(operation, body);
        log_json_message(identity, msg);
        send_json_response(*point, msg);

        // TODO: correlate response and reply to HTTP client
        resp.set_status(HttpStatus::ok);
        resp.set_body(json{{"status", "sent"}, {"uniqueId", msg.unique_id}}.dump(),
                      "application/json");
    } else {
        // SOAP: convert JSON to SOAP and send via HTTP
        json_to_soap(resp, point, operation, body);
        // For now, return acknowledgment
        resp.set_status(HttpStatus::ok);
        resp.set_body(json{{"status", "sent"}}.dump(), "application/json");
    }
}

void CSService::set_point_connected(const std::string& identity, bool value,
                                    const nlohmann::json& metadata)
{
    auto sql = fmt::format(
        "SELECT * FROM ocpp.\"SetChargePointConnected\"({}, {}, {}::jsonb)",
        pq_quote_literal(identity),
        value ? "true" : "false",
        pq_quote_literal(metadata.dump()));

    pool_.execute(std::move(sql),
        [this, identity](std::vector<PgResult> results) {
            if (results.empty() || !results[0].ok()) {
                app_.logger().error("[{}] SetChargePointConnected failed", identity);
            }
        });
}

#endif // WITH_POSTGRESQL

// ── JSON dispatch ───────────────────────────────────────────────────────────

#ifdef WITH_POSTGRESQL

void CSService::parse_json_pg(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg,
                              const std::string& account)
{
    auto sql = fmt::format(
        "SELECT * FROM ocpp.\"Parse\"({}, {}, {}, {}::jsonb, {})",
        pq_quote_literal(point.identity()),
        pq_quote_literal(msg.unique_id),
        pq_quote_literal(msg.action),
        pq_quote_literal(msg.payload.dump()),
        pq_quote_literal(account));

    auto identity = point.identity();

    pool_.execute(std::move(sql),
        [this, identity](std::vector<PgResult> results) {
            auto* point = point_manager_.find_by_identity(identity);
            if (!point) return;

            if (results.empty() || !results[0].ok() || results[0].rows() == 0) {
                app_.logger().error("[{}] ocpp.Parse() failed or returned empty", identity);
                return;
            }

            const auto& result = results[0];

            int col_type    = result.column_index("messageTypeId");
            int col_uid     = result.column_index("uniqueId");
            int col_payload = result.column_index("payload");
            int col_err     = result.column_index("errorCode");
            int col_errdesc = result.column_index("errorDescription");
            int col_details = result.column_index("errorDetails");

            ocpp::OcppMessage response;
            const char* type_id_raw = result.value(0, col_type);
            int type_id = (type_id_raw && type_id_raw[0] != '\0') ? std::stoi(type_id_raw) : 3;

            response.unique_id = result.value(0, col_uid);

            if (type_id == 4) {
                response.type = ocpp::MessageType::CallError;
                response.error_code = result.value(0, col_err);
                response.error_description = result.value(0, col_errdesc);
                const char* details = result.value(0, col_details);
                response.payload = (details && details[0] != '\0')
                    ? json::parse(details) : json::object();
            } else {
                response.type = ocpp::MessageType::CallResult;
                const char* payload_str = result.value(0, col_payload);
                response.payload = (payload_str && payload_str[0] != '\0')
                    ? json::parse(payload_str) : json::object();
            }

            send_json_response(*point, response);
        });
}

#endif // WITH_POSTGRESQL

void CSService::parse_json_webhook(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg,
                                   const std::string& account)
{
    webhook_json(point, msg, account);
}

void CSService::parse_json_standalone(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg)
{
    // Standalone mode: generate default responses in-memory
    using namespace ocpp;

    static const std::unordered_map<std::string, std::function<OcppMessage(const OcppMessage&)>> handlers = {
        {"Authorize",          CSChargingPoint::default_authorize_response},
        {"BootNotification",   CSChargingPoint::default_boot_notification_response},
        {"StartTransaction",   CSChargingPoint::default_start_transaction_response},
        {"StopTransaction",    CSChargingPoint::default_stop_transaction_response},
        {"Heartbeat",          CSChargingPoint::default_heartbeat_response},
        {"StatusNotification", CSChargingPoint::default_status_notification_response},
        {"DataTransfer",       CSChargingPoint::default_data_transfer_response},
        {"MeterValues",        CSChargingPoint::default_meter_values_response},
    };

    auto it = handlers.find(msg.action);
    if (it != handlers.end()) {
        auto response = it->second(msg);
        send_json_response(point, response);
    } else {
        auto response = ocpp::make_call_error(msg.unique_id,
            ocpp::error::NotImplemented,
            fmt::format("Action '{}' is not supported", msg.action));
        send_json_response(point, response);
    }
}

// ── Webhook ─────────────────────────────────────────────────────────────────

void CSService::webhook_json(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg,
                            const std::string& account)
{
    if (!webhook_.enabled || webhook_.url.empty()) {
        // Fallback to standalone
        parse_json_standalone(point, msg);
        return;
    }

    json webhook_payload = {
        {"identity",  point.identity()},
        {"uniqueId",  msg.unique_id},
        {"action",    msg.action},
        {"payload",   msg.payload},
        {"account",   account}
    };

    auto identity = point.identity();
    auto unique_id = msg.unique_id;

    // Build headers
    FetchClient::Headers headers;
    headers.emplace_back("Content-Type", "application/json");

    if (webhook_.auth_scheme == "Basic" && !webhook_.username.empty()) {
        auto credentials = webhook_.username + ":" + webhook_.password;
        headers.emplace_back("Authorization", "Basic " + base64_encode(credentials));
    } else if (webhook_.auth_scheme == "Bearer" && !webhook_.token.empty()) {
        headers.emplace_back("Authorization", "Bearer " + webhook_.token);
    }

    // Use FetchClient instance to POST to webhook
    if (!fetch_client_) {
        fetch_client_ = std::make_unique<FetchClient>(app_.worker_loop());
    }

    fetch_client_->post(webhook_.url, webhook_payload.dump(), headers,
        [this, identity, unique_id](FetchResponse fetch_resp) {
            auto* point = point_manager_.find_by_identity(identity);
            if (!point) return;

            if (fetch_resp.status_code < 200 || fetch_resp.status_code >= 300) {
                app_.logger().error("[{}] webhook error: HTTP {}", identity, fetch_resp.status_code);
                auto error = ocpp::make_call_error(unique_id,
                    ocpp::error::InternalError, "Webhook error");
                send_json_response(*point, error);
                return;
            }

            try {
                auto resp_json = json::parse(fetch_resp.body);

                ocpp::OcppMessage response;
                int type_id = resp_json.value("messageTypeId", 3);

                response.unique_id = resp_json.value("uniqueId", unique_id);

                if (type_id == 4) {
                    response.type = ocpp::MessageType::CallError;
                    response.error_code = resp_json.value("errorCode", "InternalError");
                    response.error_description = resp_json.value("errorDescription", "");
                    response.payload = resp_json.value("payload", json::object());
                } else {
                    response.type = ocpp::MessageType::CallResult;
                    response.payload = resp_json.value("payload", json::object());
                }

                send_json_response(*point, response);
            } catch (const std::exception& e) {
                app_.logger().error("[{}] webhook response parse error: {}",
                    identity, e.what());
                auto error = ocpp::make_call_error(unique_id,
                    ocpp::error::InternalError, "Webhook response parse error");
                send_json_response(*point, error);
            }
        },
        [this, identity, unique_id](std::string_view error) {
            app_.logger().error("[{}] webhook fetch error: {}", identity, error);
            auto* point = point_manager_.find_by_identity(identity);
            if (!point) return;
            auto err = ocpp::make_call_error(unique_id,
                ocpp::error::InternalError, "Webhook fetch error");
            send_json_response(*point, err);
        });
}

// ── Logging ─────────────────────────────────────────────────────────────────

void CSService::log_json_message(const std::string& identity, const ocpp::OcppMessage& msg)
{
    auto type_str = [](ocpp::MessageType t) -> const char* {
        switch (t) {
            case ocpp::MessageType::Call:       return "Call";
            case ocpp::MessageType::CallResult: return "CallResult";
            case ocpp::MessageType::CallError:  return "CallError";
        }
        return "Unknown";
    };

    global_logger().debug("[{}] [{}] [{}] [{}] {}",
        identity,
        msg.unique_id.empty() ? "(empty)" : msg.unique_id,
        msg.action.empty() ? "(empty)" : msg.action,
        type_str(msg.type),
        msg.payload.dump());
}

// ── Utilities ───────────────────────────────────────────────────────────────

nlohmann::json CSService::charge_point_to_json(const ocpp::CSChargingPoint& point) const
{
    json result = {
        {"identity", point.identity()},
        {"address",  point.address()},
        {"protocol", point.protocol_type() == ocpp::ProtocolType::JSON ? "JSON" : "SOAP"}
    };

    if (point.connected() && point.ws_connection()) {
        result["connection"] = {
            {"fd", point.ws_connection()->fd()}
        };
    }

    // Include last boot notification and status if available
    const auto& boot = point.last_request("BootNotification");
    if (!boot.empty() && !boot.is_null()) {
        result["bootNotification"] = boot;
    }

    const auto& status = point.last_request("StatusNotification");
    if (!status.empty() && !status.is_null()) {
        result["statusNotification"] = status;
    }

    return result;
}

nlohmann::json CSService::get_charge_point_list() const
{
    json list = json::array();
    point_manager_.for_each([&](const ocpp::CSChargingPoint& point) {
        list.push_back(charge_point_to_json(point));
    });
    return list;
}

} // namespace apostol
