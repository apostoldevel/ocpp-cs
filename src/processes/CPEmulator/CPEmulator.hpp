#pragma once

#ifdef WITH_POSTGRESQL

#include "apostol/custom_process.hpp"
#include "apostol/ws_client.hpp"
#include "ocpp/protocol.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace apostol
{

// ── CPEmulator ──────────────────────────────────────────────────────────────
//
// Charging point emulator process.
// Scans conf/cp/ for station configs, connects to CS via WebSocket,
// handles 19 CS→CP OCPP 1.6 commands, sends BootNotification on connect.
//
class CPEmulator final : public CustomProcess
{
public:
    std::string_view name() const override { return "ChargePoint"; }

    void on_start(EventLoop& loop, Application& app) override;
    void heartbeat(std::chrono::system_clock::time_point now) override;
    void on_stop() override;

private:

    // ── Station (one emulated charge point) ──────────────────────────────

    struct Station {
        std::unique_ptr<WsClient> ws;
        std::string identity;
        std::string prefix;         // cp/{name}/ directory for response files
        nlohmann::json config;      // loaded configuration.json
        std::vector<int> connector_ids;
        std::unordered_map<std::string, std::string> config_keys; // key→value

        // Track pending outbound calls (uniqueId → action)
        std::unordered_map<std::string, std::string> pending_calls;

        bool boot_sent = false;
        int  heartbeat_interval = 60; // seconds, from BootNotification response
        std::chrono::system_clock::time_point next_heartbeat{};
    };

    // ── Action handler type ──────────────────────────────────────────────

    using ActionHandler = std::function<nlohmann::json(
        Station& station, const nlohmann::json& payload)>;

    // ── Setup ────────────────────────────────────────────────────────────

    void load_stations();
    void create_station(const std::string& dir_name);
    void init_handlers();

    // ── OCPP message handling ────────────────────────────────────────────

    void on_ws_connect(Station& station);
    void on_ws_message(Station& station, uint8_t opcode, std::string payload);
    void on_ws_close(Station& station, uint16_t code, std::string_view reason);

    void send_call(Station& station, std::string_view action, nlohmann::json payload);
    void send_call_result(Station& station, std::string_view unique_id, nlohmann::json payload);
    void send_call_error(Station& station, std::string_view unique_id,
                         std::string_view code, std::string_view description);

    void handle_call(Station& station, const ocpp::OcppMessage& msg);
    void handle_call_result(Station& station, const ocpp::OcppMessage& msg);
    void handle_call_error(Station& station, const ocpp::OcppMessage& msg);

    // ── Boot sequence ────────────────────────────────────────────────────

    void send_boot_notification(Station& station);
    void send_status_notification(Station& station, int connector_id);
    void send_heartbeat(Station& station);

    // ── Response file loading (from cp/{name}/{Action}.json) ─────────────

    static nlohmann::json load_response_file(const std::string& prefix, std::string_view action);

    // ── CS→CP Action handlers ────────────────────────────────────────────

    nlohmann::json on_cancel_reservation(Station& station, const nlohmann::json& payload);
    nlohmann::json on_change_availability(Station& station, const nlohmann::json& payload);
    nlohmann::json on_change_configuration(Station& station, const nlohmann::json& payload);
    nlohmann::json on_clear_cache(Station& station, const nlohmann::json& payload);
    nlohmann::json on_clear_charging_profile(Station& station, const nlohmann::json& payload);
    nlohmann::json on_data_transfer(Station& station, const nlohmann::json& payload);
    nlohmann::json on_get_composite_schedule(Station& station, const nlohmann::json& payload);
    nlohmann::json on_get_configuration(Station& station, const nlohmann::json& payload);
    nlohmann::json on_get_diagnostics(Station& station, const nlohmann::json& payload);
    nlohmann::json on_get_local_list_version(Station& station, const nlohmann::json& payload);
    nlohmann::json on_remote_start_transaction(Station& station, const nlohmann::json& payload);
    nlohmann::json on_remote_stop_transaction(Station& station, const nlohmann::json& payload);
    nlohmann::json on_reserve_now(Station& station, const nlohmann::json& payload);
    nlohmann::json on_reset(Station& station, const nlohmann::json& payload);
    nlohmann::json on_send_local_list(Station& station, const nlohmann::json& payload);
    nlohmann::json on_set_charging_profile(Station& station, const nlohmann::json& payload);
    nlohmann::json on_trigger_message(Station& station, const nlohmann::json& payload);
    nlohmann::json on_unlock_connector(Station& station, const nlohmann::json& payload);
    nlohmann::json on_update_firmware(Station& station, const nlohmann::json& payload);

    // ── Members ──────────────────────────────────────────────────────────

    EventLoop*   loop_ = nullptr;
    Application* app_  = nullptr;
    std::string  prefix_;  // resolved config prefix (e.g. /etc/cs/)

    std::vector<std::unique_ptr<Station>> stations_;
    std::unordered_map<std::string, ActionHandler> handlers_;
};

} // namespace apostol

#endif // WITH_POSTGRESQL
