#pragma once
//
// CSService — OCPP Central System Worker Module
//
// Handles three protocols on different paths:
//   - WebSocket Upgrade on /ocpp/{identity}    — OCPP 1.6 JSON
//   - SOAP POST on /Ocpp/CentralSystemService/ — OCPP 1.5 (WITH_POSTGRESQL only)
//   - REST API on /api/v1/ChargePoint/*        — station management
//

#include "apostol/application.hpp"
#include "apostol/apostol_module.hpp"
#include "apostol/websocket.hpp"
#ifdef WITH_POSTGRESQL
#include "apostol/pg.hpp"
#include "apostol/pg_exec.hpp"
#endif

#include "ocpp/protocol.hpp"
#include "ocpp/charging_point.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>

namespace apostol
{

class FetchClient;  // forward declaration — destructor defined in CSService.cpp

// ── Webhook config ──────────────────────────────────────────────────────────

struct WebhookConfig {
    bool        enabled = false;
    std::string url;
    std::string auth_scheme;   // "Basic" | "Bearer" | "Off"
    std::string username;
    std::string password;
    std::string token;
};

// ── PendingCall — correlates outbound CS→CP Call with deferred HTTP response ─

struct PendingCall {
    std::shared_ptr<HttpConnection> conn;     // deferred HTTP connection
    std::string                     action;   // OCPP action name
    std::chrono::steady_clock::time_point deadline;
};

// ── CSService ───────────────────────────────────────────────────────────────

class CSService final : public ApostolModule
{
public:
    explicit CSService(Application& app);
    ~CSService() override;

    std::string_view name() const override { return "CSService"; }
    std::string_view title() const override { return "ocpp central system service"; }
    bool enabled() const override { return enabled_; }
    bool check_location(const HttpRequest& req) const override;

protected:
    void init_methods() override;

private:

    // ── HTTP handlers ───────────────────────────────────────────────────

    void do_get(const HttpRequest& req, HttpResponse& resp);
    void do_post(const HttpRequest& req, HttpResponse& resp);

    // ── WebSocket (OCPP 1.6 JSON) ───────────────────────────────────────

    void on_ws_upgrade(EventLoop& loop, WsConnection ws, const HttpRequest& req);
    void on_ws_message(ocpp::CSChargingPoint& point, const std::string& payload);
    void on_ws_close(const std::string& identity);

    void send_json_response(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& response);

    // ── SOAP (OCPP 1.5) — WITH_POSTGRESQL only ──────────────────────────

#ifdef WITH_POSTGRESQL
    void do_soap(const HttpRequest& req, HttpResponse& resp);
    void parse_soap(const HttpRequest& req, HttpResponse& resp, const std::string& payload);
    void json_to_soap(const HttpRequest& req, HttpResponse& resp,
                      ocpp::CSChargingPoint* point,
                      const std::string& operation, const nlohmann::json& payload);
    void soap_to_json(std::shared_ptr<HttpConnection> conn, const std::string& xml_payload);
#endif

    // ── REST API (/api/v1/ChargePoint/*) ────────────────────────────────

    void do_api(const HttpRequest& req, HttpResponse& resp);
    void do_charge_point_list(const HttpRequest& req, HttpResponse& resp);

    void do_charge_point(const HttpRequest& req, HttpResponse& resp,
                        const std::string& identity, const std::string& operation);

#ifdef WITH_POSTGRESQL
    void do_central_system(const HttpRequest& req, HttpResponse& resp,
                          const std::string& endpoint);
    void set_point_connected(const std::string& identity, bool value,
                            const nlohmann::json& metadata);
#endif

    // ── JSON dispatch (PG or webhook or standalone) ─────────────────────

#ifdef WITH_POSTGRESQL
    void parse_json_pg(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg,
                       const std::string& account = {});
#endif
    void parse_json_webhook(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg,
                           const std::string& account = {});
    void parse_json_standalone(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg);

    // ── Webhook ─────────────────────────────────────────────────────────

    void webhook_json(ocpp::CSChargingPoint& point, const ocpp::OcppMessage& msg,
                     const std::string& account = {});

    // ── Utilities ───────────────────────────────────────────────────────

    static void log_json_message(const std::string& identity, const ocpp::OcppMessage& msg);

    nlohmann::json charge_point_to_json(const ocpp::CSChargingPoint& point) const;
    nlohmann::json get_charge_point_list() const;

    // ── Pending call management ─────────────────────────────────────────

    void cleanup_expired_calls();

    // ── Members ─────────────────────────────────────────────────────────

    Application&                    app_;
#ifdef WITH_POSTGRESQL
    PgPool*                         pool_ {nullptr};
#endif
    ocpp::CSChargingPointManager    point_manager_;
    WebhookConfig                   webhook_;
    bool                            enabled_;
    bool                            api_auth_ = false; // true = production (JWT required)

    // WsConnection storage: fd -> WsConnection (moved here after upgrade)
    std::unordered_map<int, WsConnection> ws_connections_;

    // Browser WebSocket log subscribers: fd -> WsConnection
    std::unordered_map<int, WsConnection> log_subscribers_;
    void broadcast_log(const nlohmann::json& entry);

    // Lazy-initialized FetchClient for webhook dispatch
    std::unique_ptr<FetchClient> fetch_client_;

    // Pending outbound calls: uniqueId -> PendingCall (deferred HTTP response)
    std::unordered_map<std::string, PendingCall> pending_calls_;

    static constexpr std::chrono::seconds pending_call_timeout_{30};
};

} // namespace apostol
