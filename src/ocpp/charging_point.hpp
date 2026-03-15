#pragma once
//
// Server-side Charging Point Manager
//
// Manages the registry of connected charging stations (OCPP 1.5 SOAP + 1.6 JSON).
// Each CSChargingPoint tracks connection state, identity, protocol type,
// and last-received request data for each OCPP operation.
//

#include "ocpp/protocol.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <chrono>

namespace apostol { class WsConnection; }

namespace ocpp
{

// ── CSChargingPoint ─────────────────────────────────────────────────────────
// Represents a single charging station connected to this Central System.
// One instance per station identity (e.g. "STRK-ION-001").

class CSChargingPointManager;

class CSChargingPoint
{
public:
    explicit CSChargingPoint(std::string identity);
    ~CSChargingPoint() = default;

    CSChargingPoint(const CSChargingPoint&) = delete;
    CSChargingPoint& operator=(const CSChargingPoint&) = delete;

    // ── Identity & addressing ───────────────────────────────────────────

    const std::string& identity() const { return identity_; }

    const std::string& address() const { return address_; }
    void set_address(std::string addr) { address_ = std::move(addr); }

    const std::string& ocpp_version() const { return ocpp_version_; }
    void set_ocpp_version(std::string version) { ocpp_version_ = std::move(version); }

    // ── Connection state ────────────────────────────────────────────────

    ProtocolType protocol_type() const { return protocol_type_; }
    void set_protocol_type(ProtocolType pt) { protocol_type_ = pt; }

    bool connected() const { return ws_conn_ != nullptr; }

    apostol::WsConnection* ws_connection() const { return ws_conn_; }
    void set_ws_connection(apostol::WsConnection* conn) { ws_conn_ = conn; }

    // ── Send OCPP JSON message over WebSocket ───────────────────────────

    void send_json(const OcppMessage& msg);
    void send_call_error(std::string_view unique_id, std::string_view code, std::string_view desc);

    // ── Last request data (populated during message parsing) ────────────
    // Stored as raw JSON for flexibility — CSService or PG dispatch can use them.

    nlohmann::json& last_request(std::string_view action);
    const nlohmann::json& last_request(std::string_view action) const;
    void store_request(std::string_view action, nlohmann::json payload);

    // ── Timestamps ──────────────────────────────────────────────────────

    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;

    time_point connected_at() const { return connected_at_; }
    void set_connected_at(time_point tp) { connected_at_ = tp; }

    time_point last_seen() const { return last_seen_; }
    void touch() { last_seen_ = clock::now(); }

    // ── Standalone (no-PG) default response handlers ────────────────────
    // Generate default "Accepted" responses for all OCPP operations.
    // Used when WITH_POSTGRESQL is OFF (webhook mode).

    static OcppMessage default_authorize_response(const OcppMessage& request);
    static OcppMessage default_boot_notification_response(const OcppMessage& request);
    static OcppMessage default_start_transaction_response(const OcppMessage& request);
    static OcppMessage default_stop_transaction_response(const OcppMessage& request);
    static OcppMessage default_heartbeat_response(const OcppMessage& request);
    static OcppMessage default_status_notification_response(const OcppMessage& request);
    static OcppMessage default_data_transfer_response(const OcppMessage& request);
    static OcppMessage default_meter_values_response(const OcppMessage& request);

private:
    std::string identity_;
    std::string address_;
    std::string ocpp_version_ = "1.6";
    ProtocolType protocol_type_ = ProtocolType::JSON;

    apostol::WsConnection* ws_conn_ = nullptr;

    // Last request payload per action (e.g. "BootNotification" -> {...})
    std::unordered_map<std::string, nlohmann::json> last_requests_;

    time_point connected_at_{};
    time_point last_seen_{};
};

// ── CSChargingPointManager ──────────────────────────────────────────────────
// Registry of connected charging stations. Thread-safe is NOT required
// (single epoll thread per worker).

class CSChargingPointManager
{
public:
    CSChargingPointManager() = default;

    // Add or get existing point by identity. Creates if not found.
    CSChargingPoint& get_or_create(const std::string& identity);

    // Find by identity (nullptr if not found).
    CSChargingPoint* find_by_identity(std::string_view identity) const;

    // Find by WsConnection (nullptr if not found).
    CSChargingPoint* find_by_connection(const apostol::WsConnection* conn) const;

    // Remove by identity. Returns true if found and removed.
    bool remove(const std::string& identity);

    // Iteration.
    std::size_t size() const { return points_.size(); }
    bool empty() const { return points_.empty(); }

    template<typename Fn>
    void for_each(Fn&& fn) const
    {
        for (const auto& [id, pt] : points_)
            fn(*pt);
    }

private:
    std::unordered_map<std::string, std::unique_ptr<CSChargingPoint>> points_;
};

} // namespace ocpp
