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

    enum class BootStatus { not_sent, pending, accepted, rejected };

    // ── IdTagInfo — authorization result for an idTag ──────────────────────
    struct IdTagInfo {
        std::string status = "Invalid";  // Accepted|Blocked|Expired|Invalid|ConcurrentTx
        std::string parent_id_tag;
        std::chrono::system_clock::time_point expiry_date{};
    };

    // ── ConnectorState — per-connector state (v1 CChargingPointConnector) ──
    struct ConnectorState {
        int connector_id = 0;
        int transaction_id = -1;      // -1 = no active transaction
        int reservation_id = -1;      // -1 = no reservation
        int meter_value = 0;          // Wh, incremented during Charging
        std::string status = "Available";
        std::string id_tag;           // idTag of current transaction
        std::string reservation_id_tag;
        std::chrono::system_clock::time_point status_updated{};
        std::chrono::system_clock::time_point reservation_expiry{};

        void reset() {
            transaction_id = -1;
            reservation_id = -1;
            meter_value = 0;
            status = "Available";
            id_tag.clear();
            reservation_id_tag.clear();
            status_updated = {};
            reservation_expiry = {};
        }
    };

    struct Station {
        std::unique_ptr<WsClient> ws;
        std::string identity;
        std::string prefix;         // cp/{name}/ directory for response files
        nlohmann::json config;      // loaded configuration.json
        std::vector<int> connector_ids;
        struct ConfigValue {
            std::string value;
            bool readonly = false;
        };
        std::unordered_map<std::string, ConfigValue> config_keys; // key→{value,readonly}

        // Track pending outbound calls (uniqueId → action + deadline)
        struct PendingCall {
            std::string action;
            std::chrono::system_clock::time_point deadline;
            int connector_id = 0;   // for StartTransaction/StopTransaction/Authorize correlation
            std::string id_tag;     // for Authorize/StartTransaction — needed in CallResult handler
        };
        std::unordered_map<std::string, PendingCall> pending_calls;

        // Per-connector state (parallel to connector_ids)
        std::vector<ConnectorState> connector_states;

        // Authorization cache: idTag → IdTagInfo
        std::unordered_map<std::string, IdTagInfo> auth_cache;

        BootStatus boot_status = BootStatus::not_sent;
        int  heartbeat_interval = 60; // seconds, from BootNotification response
        std::chrono::system_clock::time_point next_heartbeat{};
        std::chrono::system_clock::time_point next_boot_retry{};
        std::chrono::system_clock::time_point next_meter_values{};
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

    void send_call(Station& station, std::string_view action, nlohmann::json payload,
                   int connector_id = 0, const std::string& id_tag = {});
    void send_call_result(Station& station, std::string_view unique_id, nlohmann::json payload);
    void send_call_error(Station& station, std::string_view unique_id,
                         std::string_view code, std::string_view description);

    void handle_call(Station& station, const ocpp::OcppMessage& msg);
    void handle_call_result(Station& station, const ocpp::OcppMessage& msg);
    void handle_call_error(Station& station, const ocpp::OcppMessage& msg);

    // ── Boot sequence ────────────────────────────────────────────────────

    void send_boot_notification(Station& station);
    void send_status_notification(Station& station, int connector_id);
    void send_status_notification(Station& station, int connector_id, const std::string& status);
    void send_heartbeat(Station& station);

    // ── Charging session helpers (v1 parity) ──────────────────────────

    static int status_order(const std::string& s);

    ConnectorState* find_connector(Station& station, int connector_id);
    ConnectorState* find_connector_by_transaction(Station& station, int transaction_id);

    IdTagInfo authorize_local(Station& station, const std::string& id_tag);
    void update_auth_cache(Station& station, const std::string& id_tag, const IdTagInfo& info);
    static IdTagInfo parse_id_tag_info(const nlohmann::json& payload);

    void continue_remote_start(Station& station, ConnectorState& conn);
    void send_start_transaction(Station& station, ConnectorState& conn);
    void local_stop_transaction(Station& station, ConnectorState& conn, const std::string& reason);
    void send_meter_values(Station& station, ConnectorState& conn);

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
