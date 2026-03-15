#include "ocpp/charging_point.hpp"
#include "ocpp/time_utils.hpp"
#include "apostol/websocket.hpp"
#include <fmt/format.h>
#include <chrono>
#include <atomic>

namespace ocpp
{

// ── Helpers ─────────────────────────────────────────────────────────────────

namespace
{
std::atomic<uint32_t> s_transaction_id{0};
constexpr int kDefaultExpirySec = 5 * 60; // 5 min default for idTagInfo/reservation expiry
} // namespace

// ── CSChargingPoint ─────────────────────────────────────────────────────────

CSChargingPoint::CSChargingPoint(std::string identity)
    : identity_(std::move(identity))
{
}

void CSChargingPoint::send_json(const OcppMessage& msg)
{
    if (ws_conn_) {
        ws_conn_->send_text(serialize_ocpp_json(msg));
    }
}

void CSChargingPoint::send_call_error(std::string_view unique_id, std::string_view code, std::string_view desc)
{
    send_json(make_call_error(unique_id, code, desc));
}

nlohmann::json& CSChargingPoint::last_request(std::string_view action)
{
    return last_requests_[std::string(action)];
}

const nlohmann::json& CSChargingPoint::last_request(std::string_view action) const
{
    static const nlohmann::json empty = nlohmann::json::object();
    auto it = last_requests_.find(std::string(action));
    return it != last_requests_.end() ? it->second : empty;
}

void CSChargingPoint::store_request(std::string_view action, nlohmann::json payload)
{
    last_requests_[std::string(action)] = std::move(payload);
}

// ── Default response handlers (standalone / webhook mode) ───────────────────

OcppMessage CSChargingPoint::default_authorize_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, {
        {"idTagInfo", {
            {"status", "Accepted"},
            {"expiryDate", iso_time_now(kDefaultExpirySec)}
        }}
    });
}

OcppMessage CSChargingPoint::default_boot_notification_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, {
        {"status", "Accepted"},
        {"currentTime", iso_time_now()},
        {"interval", 60}
    });
}

OcppMessage CSChargingPoint::default_start_transaction_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, {
        {"idTagInfo", {
            {"status", "Accepted"},
            {"expiryDate", iso_time_now(kDefaultExpirySec)}
        }},
        {"transactionId", ++s_transaction_id}
    });
}

OcppMessage CSChargingPoint::default_stop_transaction_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, {
        {"idTagInfo", {
            {"status", "Accepted"},
            {"expiryDate", iso_time_now(kDefaultExpirySec)}
        }}
    });
}

OcppMessage CSChargingPoint::default_heartbeat_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, {
        {"currentTime", iso_time_now()}
    });
}

OcppMessage CSChargingPoint::default_status_notification_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, nlohmann::json::object());
}

OcppMessage CSChargingPoint::default_data_transfer_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, {
        {"status", "Accepted"}
    });
}

OcppMessage CSChargingPoint::default_meter_values_response(const OcppMessage& request)
{
    return make_call_result(request.unique_id, nlohmann::json::object());
}

// ── CSChargingPointManager ──────────────────────────────────────────────────

CSChargingPoint& CSChargingPointManager::get_or_create(const std::string& identity)
{
    auto it = points_.find(identity);
    if (it != points_.end())
        return *it->second;

    auto [inserted, ok] = points_.emplace(identity, std::make_unique<CSChargingPoint>(identity));
    return *inserted->second;
}

CSChargingPoint* CSChargingPointManager::find_by_identity(std::string_view identity) const
{
    auto it = points_.find(std::string(identity));
    return it != points_.end() ? it->second.get() : nullptr;
}

CSChargingPoint* CSChargingPointManager::find_by_connection(const apostol::WsConnection* conn) const
{
    for (const auto& [id, pt] : points_) {
        if (pt->ws_connection() == conn)
            return pt.get();
    }
    return nullptr;
}

bool CSChargingPointManager::remove(const std::string& identity)
{
    return points_.erase(identity) > 0;
}

} // namespace ocpp
