#ifdef WITH_POSTGRESQL

#include "CPEmulator.hpp"
#include "apostol/application.hpp"
#include "apostol/http_utils.hpp"
#include "ocpp/ocpp_codec.hpp"
#include "apostol/logger.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <chrono>

namespace apostol
{

namespace fs = std::filesystem;

// ── Lifecycle ────────────────────────────────────────────────────────────────

void CPEmulator::on_start(EventLoop& loop, Application& app)
{
    loop_ = &loop;
    app_  = &app;

    prefix_ = app.settings().prefix;
    if (!prefix_.empty() && prefix_.back() != '/')
        prefix_ += '/';

    init_handlers();
    load_stations();
}

void CPEmulator::heartbeat(std::chrono::system_clock::time_point now)
{
    for (auto& station : stations_) {
        if (!station->ws || !station->ws->connected())
            continue;

        // Boot sequence: send/retry BootNotification until Accepted
        if (station->boot_status != BootStatus::accepted) {
            if (station->boot_status == BootStatus::not_sent ||
                (station->next_boot_retry != std::chrono::system_clock::time_point{} &&
                 now >= station->next_boot_retry)) {
                send_boot_notification(*station);
                station->boot_status = BootStatus::pending;
                station->next_boot_retry = {};  // will be set by response handler
            }
            continue;  // no heartbeat until boot Accepted (OCPP 1.6 §4.2)
        }

        // ── Per-connector logic (v1 parity) ───────────────────────────────

        for (auto& conn : station->connector_states) {
            if (conn.status == "Charging") {
                conn.meter_value += 10;  // +10 Wh per tick
            } else if (conn.status == "Finishing") {
                // Finishing → Available after timeout
                auto cfg_it = station->config_keys.find("FinishingTimeout");
                int timeout = 60;
                if (cfg_it != station->config_keys.end() && !cfg_it->second.value.empty()) {
                    try { timeout = std::stoi(cfg_it->second.value); } catch (...) {}
                }
                if (timeout > 0 && conn.status_updated != std::chrono::system_clock::time_point{} &&
                    now >= conn.status_updated + std::chrono::seconds(timeout)) {
                    conn.status = "Available";
                    conn.status_updated = now;
                    send_status_notification(*station, conn.connector_id, "Available");
                }
            } else if (conn.status == "Reserved") {
                // Cancel expired reservation
                if (conn.reservation_expiry != std::chrono::system_clock::time_point{} &&
                    now >= conn.reservation_expiry) {
                    app_->logger().info("[{}] reservation {} expired on connector {}",
                        station->identity, conn.reservation_id, conn.connector_id);
                    conn.reservation_id = -1;
                    conn.reservation_id_tag.clear();
                    conn.reservation_expiry = {};
                    conn.status = "Available";
                    conn.status_updated = now;
                    send_status_notification(*station, conn.connector_id, "Available");
                }
            }
        }

        // ── MeterValues (every 60 sec) ────────────────────────────────────

        bool meter_sent = false;
        if (now >= station->next_meter_values) {
            station->next_meter_values = now + std::chrono::seconds(60);
            for (auto& conn : station->connector_states) {
                if (conn.status == "Charging" ||
                    conn.status == "SuspendedEVSE" ||
                    conn.status == "SuspendedEV") {
                    send_meter_values(*station, conn);
                    meter_sent = true;
                }
            }
        }

        // ── Heartbeat (only if no MeterValues sent this tick) ─────────────

        if (!meter_sent && now >= station->next_heartbeat) {
            send_heartbeat(*station);
            station->next_heartbeat = now +
                std::chrono::seconds(station->heartbeat_interval);
        }

        // ── Cleanup expired pending calls (30s timeout) ───────────────────

        for (auto it = station->pending_calls.begin(); it != station->pending_calls.end(); ) {
            if (now >= it->second.deadline) {
                app_->logger().warn("[{}] pending call {} ({}) timed out",
                    station->identity, it->first, it->second.action);
                it = station->pending_calls.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void CPEmulator::on_stop()
{
    for (auto& station : stations_) {
        if (station->ws && station->ws->connected())
            station->ws->close(1000, "shutdown");
    }
    stations_.clear();
}

// ── Station loading ──────────────────────────────────────────────────────────

void CPEmulator::load_stations()
{
    auto cp_dir = prefix_ + "cp";
    if (!fs::is_directory(cp_dir)) {
        app_->logger().warn("CPEmulator: directory '{}' not found, no stations", cp_dir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(cp_dir)) {
        if (!entry.is_directory())
            continue;
        try {
            create_station(entry.path().filename().string());
        } catch (const std::exception& e) {
            app_->logger().error("CPEmulator: failed to create station '{}': {}",
                                  entry.path().filename().string(), e.what());
        }
    }

    app_->logger().info("CPEmulator: loaded {} station(s)", stations_.size());
}

void CPEmulator::create_station(const std::string& dir_name)
{
    auto station_dir = prefix_ + "cp/" + dir_name + "/";
    auto config_file = station_dir + "configuration.json";

    if (!fs::exists(config_file))
        throw std::runtime_error(fmt::format("configuration file not found: {}", config_file));

    // Load configuration.json
    std::ifstream ifs(config_file);
    nlohmann::json config;
    ifs >> config;

    // Parse connector IDs — each entry is either an int or {"id": N, ...}
    std::vector<int> connector_ids;
    if (config.contains("connectorId") && config["connectorId"].is_array()) {
        for (const auto& cid : config["connectorId"]) {
            if (cid.is_number())
                connector_ids.push_back(cid.get<int>());
            else if (cid.is_object() && cid.contains("id"))
                connector_ids.push_back(cid["id"].get<int>());
        }
    }

    // Parse configuration keys (preserving readonly flag)
    std::unordered_map<std::string, Station::ConfigValue> config_keys;
    if (config.contains("configurationKey") && config["configurationKey"].is_array()) {
        for (const auto& entry : config["configurationKey"]) {
            if (entry.contains("key") && entry.contains("value"))
                config_keys[entry["key"].get<std::string>()] = {
                    entry["value"].get<std::string>(),
                    entry.value("readonly", false)
                };
        }
    }

    auto url_it = config_keys.find("CentralSystemURL");
    if (url_it == config_keys.end() || url_it->second.value.empty())
        throw std::runtime_error(fmt::format("[{}] CentralSystemURL not found in configuration", dir_name));

    auto identity_it = config_keys.find("StationIdentity");
    std::string identity = identity_it != config_keys.end() ? identity_it->second.value : dir_name;

    app_->logger().info("[{}] configuration: {}", identity, config_file);

    // Create station
    auto station = std::make_unique<Station>();
    station->identity     = identity;
    station->prefix       = station_dir;
    station->config       = std::move(config);
    station->connector_ids = connector_ids;  // keep a copy before move
    station->config_keys  = std::move(config_keys);

    // Initialize per-connector state
    for (int cid : connector_ids)
        station->connector_states.push_back(ConnectorState{.connector_id = cid});

    // Create WsClient with OcppCodec
    station->ws = std::make_unique<WsClient>(*loop_);
    station->ws->set_codec(std::make_unique<ocpp::OcppCodec>());
    station->ws->auto_reconnect(true);
    station->ws->set_reconnect_max_delay(std::chrono::seconds(30));
    station->ws->set_ping_interval(std::chrono::seconds(30));

    auto* st = station.get();

    station->ws->on_connect([this, st] {
        on_ws_connect(*st);
    });

    station->ws->on_message([this, st](uint8_t opcode, std::string payload) {
        on_ws_message(*st, opcode, std::move(payload));
    });

    station->ws->on_close([this, st](uint16_t code, std::string_view reason) {
        on_ws_close(*st, code, reason);
    });

    station->ws->on_error([this, st](std::string_view err) {
        app_->logger().error("[{}] WS error: {}", st->identity, err);
    });

    // Build URL: ws://host/path/identity
    auto ws_url = url_it->second.value;
    if (!ws_url.empty() && ws_url.back() != '/')
        ws_url += '/';
    ws_url += url_encode(identity);

    app_->logger().info("[{}] connecting to {}", identity, ws_url);
    station->ws->connect(ws_url, {"ocpp1.6"});

    stations_.push_back(std::move(station));
}

// ── Handler initialization ───────────────────────────────────────────────────

void CPEmulator::init_handlers()
{
    handlers_["CancelReservation"]      = [this](auto& s, auto& p) { return on_cancel_reservation(s, p); };
    handlers_["ChangeAvailability"]     = [this](auto& s, auto& p) { return on_change_availability(s, p); };
    handlers_["ChangeConfiguration"]    = [this](auto& s, auto& p) { return on_change_configuration(s, p); };
    handlers_["ClearCache"]             = [this](auto& s, auto& p) { return on_clear_cache(s, p); };
    handlers_["ClearChargingProfile"]   = [this](auto& s, auto& p) { return on_clear_charging_profile(s, p); };
    handlers_["DataTransfer"]           = [this](auto& s, auto& p) { return on_data_transfer(s, p); };
    handlers_["GetCompositeSchedule"]   = [this](auto& s, auto& p) { return on_get_composite_schedule(s, p); };
    handlers_["GetConfiguration"]       = [this](auto& s, auto& p) { return on_get_configuration(s, p); };
    handlers_["GetDiagnostics"]         = [this](auto& s, auto& p) { return on_get_diagnostics(s, p); };
    handlers_["GetLocalListVersion"]    = [this](auto& s, auto& p) { return on_get_local_list_version(s, p); };
    handlers_["RemoteStartTransaction"] = [this](auto& s, auto& p) { return on_remote_start_transaction(s, p); };
    handlers_["RemoteStopTransaction"]  = [this](auto& s, auto& p) { return on_remote_stop_transaction(s, p); };
    handlers_["ReserveNow"]             = [this](auto& s, auto& p) { return on_reserve_now(s, p); };
    handlers_["Reset"]                  = [this](auto& s, auto& p) { return on_reset(s, p); };
    handlers_["SendLocalList"]          = [this](auto& s, auto& p) { return on_send_local_list(s, p); };
    handlers_["SetChargingProfile"]     = [this](auto& s, auto& p) { return on_set_charging_profile(s, p); };
    handlers_["TriggerMessage"]         = [this](auto& s, auto& p) { return on_trigger_message(s, p); };
    handlers_["UnlockConnector"]        = [this](auto& s, auto& p) { return on_unlock_connector(s, p); };
    handlers_["UpdateFirmware"]         = [this](auto& s, auto& p) { return on_update_firmware(s, p); };
}

// ── WebSocket callbacks ──────────────────────────────────────────────────────

void CPEmulator::on_ws_connect(Station& station)
{
    app_->logger().info("[{}] connected", station.identity);
    station.boot_status = BootStatus::not_sent;
    station.next_boot_retry = {};
}

void CPEmulator::on_ws_message(Station& station, uint8_t opcode, std::string payload)
{
    if (opcode != 0x01) // text only
        return;

    try {
        auto msg = ocpp::parse_ocpp_json(payload);

        app_->logger().debug("[{}] [{}] [{}] {}",
            station.identity, msg.unique_id,
            msg.action.empty() ? "Response" : msg.action,
            msg.payload.dump());

        switch (msg.type) {
        case ocpp::MessageType::Call:
            handle_call(station, msg);
            break;
        case ocpp::MessageType::CallResult:
            handle_call_result(station, msg);
            break;
        case ocpp::MessageType::CallError:
            handle_call_error(station, msg);
            break;
        }
    } catch (const std::exception& e) {
        app_->logger().error("[{}] parse error: {}", station.identity, e.what());
    }
}

void CPEmulator::on_ws_close(Station& station, uint16_t code, std::string_view reason)
{
    app_->logger().info("[{}] disconnected: {} {}", station.identity, code, reason);
    station.boot_status = BootStatus::not_sent;
    station.next_boot_retry = {};
    station.next_meter_values = {};
    station.pending_calls.clear();

    // Reset all connectors on disconnect
    for (auto& conn : station.connector_states)
        conn.reset();
}

// ── OCPP message dispatch ────────────────────────────────────────────────────

void CPEmulator::handle_call(Station& station, const ocpp::OcppMessage& msg)
{
    auto it = handlers_.find(msg.action);
    if (it == handlers_.end()) {
        send_call_error(station, msg.unique_id, ocpp::error::NotImplemented,
                        fmt::format("Action '{}' not supported", msg.action));
        return;
    }

    try {
        auto response_payload = it->second(station, msg.payload);
        send_call_result(station, msg.unique_id, std::move(response_payload));
    } catch (const std::exception& e) {
        send_call_error(station, msg.unique_id, ocpp::error::InternalError, e.what());
    }
}

void CPEmulator::handle_call_result(Station& station, const ocpp::OcppMessage& msg)
{
    auto it = station.pending_calls.find(msg.unique_id);
    if (it == station.pending_calls.end()) {
        app_->logger().warn("[{}] unexpected CallResult for id={}", station.identity, msg.unique_id);
        return;
    }

    auto pending = std::move(it->second);
    station.pending_calls.erase(it);

    auto now = std::chrono::system_clock::now();

    // ── BootNotification (OCPP 1.6 §4.2) ─────────────────────────────────
    if (pending.action == "BootNotification") {
        auto status = msg.payload.value("status", "Rejected");
        auto interval = msg.payload.value("interval", 60);
        app_->logger().info("[{}] BootNotification: status={} interval={}",
                            station.identity, status, interval);
        station.heartbeat_interval = interval;

        if (status == "Accepted") {
            station.boot_status = BootStatus::accepted;
            station.next_heartbeat = now + std::chrono::seconds(interval);
            // Send initial StatusNotification for whole CP (connectorId=0), then each connector
            send_status_notification(station, 0);
            for (auto& conn : station.connector_states)
                send_status_notification(station, conn.connector_id, conn.status);
        } else {
            station.boot_status = (status == "Pending")
                ? BootStatus::pending : BootStatus::rejected;
            station.next_boot_retry = now + std::chrono::seconds(interval);
            app_->logger().info("[{}] will retry BootNotification in {}s",
                                station.identity, interval);
        }
        return;
    }

    // ── Authorize ─────────────────────────────────────────────────────────
    if (pending.action == "Authorize") {
        auto info = parse_id_tag_info(msg.payload);
        update_auth_cache(station, pending.id_tag, info);

        auto* conn = find_connector(station, pending.connector_id);
        if (!conn) return;

        if (info.status == "Accepted") {
            continue_remote_start(station, *conn);
        } else {
            app_->logger().warn("[{}] Authorize rejected for {} on connector {}",
                station.identity, pending.id_tag, conn->connector_id);
            conn->status = "Faulted";
            conn->status_updated = now;
            send_status_notification(station, conn->connector_id, "Faulted");
        }
        return;
    }

    // ── StartTransaction ──────────────────────────────────────────────────
    if (pending.action == "StartTransaction") {
        auto* conn = find_connector(station, pending.connector_id);
        if (!conn) return;

        // Parse idTagInfo if present
        IdTagInfo info;
        bool has_id_tag_info = msg.payload.contains("idTagInfo");
        if (has_id_tag_info) {
            info = parse_id_tag_info(msg.payload);
            update_auth_cache(station, pending.id_tag, info);
        }

        // Save transactionId
        conn->transaction_id = msg.payload.value("transactionId", -1);

        app_->logger().info("[{}] StartTransaction: transactionId={} connector={}",
            station.identity, conn->transaction_id, conn->connector_id);

        if (!has_id_tag_info || info.status == "Accepted") {
            // Determine charging status from config or default "Charging"
            std::string charge_status = "Charging";
            auto cfg_it = station.config_keys.find("StartTransactionStatus");
            if (cfg_it != station.config_keys.end() && !cfg_it->second.value.empty())
                charge_status = cfg_it->second.value;

            conn->status = charge_status;
            conn->status_updated = now;
            send_status_notification(station, conn->connector_id, charge_status);
        } else {
            // Authorization rejected — stop transaction immediately
            app_->logger().warn("[{}] StartTransaction auth rejected: {}",
                station.identity, info.status);
            local_stop_transaction(station, *conn, info.status);
        }
        return;
    }

    // ── StopTransaction ───────────────────────────────────────────────────
    if (pending.action == "StopTransaction") {
        auto* conn = find_connector(station, pending.connector_id);
        if (!conn) return;

        if (msg.payload.contains("idTagInfo")) {
            auto info = parse_id_tag_info(msg.payload);
            update_auth_cache(station, pending.id_tag, info);
        }

        conn->status = "Finishing";
        conn->status_updated = now;
        send_status_notification(station, conn->connector_id, "Finishing");
        return;
    }
}

void CPEmulator::handle_call_error(Station& station, const ocpp::OcppMessage& msg)
{
    auto it = station.pending_calls.find(msg.unique_id);
    std::string action = "unknown";
    if (it != station.pending_calls.end()) {
        action = std::move(it->second.action);
        station.pending_calls.erase(it);
    }

    app_->logger().warn("[{}] CallError for {}: {} - {}",
        station.identity, action, msg.error_code, msg.error_description);
}

// ── Sending helpers ──────────────────────────────────────────────────────────

void CPEmulator::send_call(Station& station, std::string_view action, nlohmann::json payload,
                            int connector_id, const std::string& id_tag)
{
    auto msg = ocpp::make_call(action, std::move(payload));
    station.pending_calls[msg.unique_id] = Station::PendingCall{
        .action       = std::string(action),
        .deadline     = std::chrono::system_clock::now() + std::chrono::seconds(30),
        .connector_id = connector_id,
        .id_tag       = id_tag
    };
    station.ws->send_text(ocpp::serialize_ocpp_json(msg));
}

void CPEmulator::send_call_result(Station& station, std::string_view unique_id, nlohmann::json payload)
{
    auto msg = ocpp::make_call_result(unique_id, std::move(payload));
    station.ws->send_text(ocpp::serialize_ocpp_json(msg));
}

void CPEmulator::send_call_error(Station& station, std::string_view unique_id,
                                  std::string_view code, std::string_view description)
{
    auto msg = ocpp::make_call_error(unique_id, code, description);
    station.ws->send_text(ocpp::serialize_ocpp_json(msg));
}

// ── Boot sequence ────────────────────────────────────────────────────────────

void CPEmulator::send_boot_notification(Station& station)
{
    nlohmann::json payload;

    // Try to load from file, fallback to configurationKey values (as in v1)
    auto file_payload = load_response_file(station.prefix, "BootNotification");
    if (!file_payload.is_null()) {
        payload = std::move(file_payload);
    } else {
        auto& keys = station.config_keys;
        auto require_key = [&](const std::string& k) -> const std::string& {
            auto it = keys.find(k);
            if (it == keys.end() || it->second.value.empty())
                throw std::runtime_error(fmt::format("[{}] missing required configurationKey: {}", station.identity, k));
            return it->second.value;
        };
        payload = {
            {"chargePointVendor",       require_key("ChargePointVendor")},
            {"chargePointModel",        require_key("ChargePointModel")},
            {"chargePointSerialNumber", require_key("ChargePointSerialNumber")},
            {"firmwareVersion",         require_key("ChargePointSoftwareVersion")}
        };
    }

    app_->logger().info("[{}] sending BootNotification", station.identity);
    send_call(station, "BootNotification", std::move(payload));
}

void CPEmulator::send_status_notification(Station& station, int connector_id)
{
    // Determine actual status from connector state
    std::string status = "Available";
    auto* conn = find_connector(station, connector_id);
    if (conn)
        status = conn->status;

    send_status_notification(station, connector_id, status);
}

void CPEmulator::send_heartbeat(Station& station)
{
    send_call(station, "Heartbeat", nlohmann::json::object());
}

// ── Response file loading ────────────────────────────────────────────────────

nlohmann::json CPEmulator::load_response_file(const std::string& prefix, std::string_view action)
{
    auto path = prefix + std::string(action) + ".json";
    if (!fs::exists(path))
        return nlohmann::json();

    std::ifstream ifs(path);
    nlohmann::json j;
    ifs >> j;
    return j;
}

// ── Charging session helpers (v1 parity) ─────────────────────────────────────

namespace
{

std::string iso_time_now()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm utc{};
    gmtime_r(&tt, &utc);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &utc);
    return fmt::format("{}.{:03d}Z", buf, ms.count());
}

std::chrono::system_clock::time_point parse_iso_time(const std::string& s)
{
    std::tm tm{};
    if (strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &tm))
        return std::chrono::system_clock::from_time_t(timegm(&tm));
    return {};
}

} // namespace

int CPEmulator::status_order(const std::string& s)
{
    if (s == "Available")     return 0;
    if (s == "Preparing")     return 1;
    if (s == "Charging")      return 2;
    if (s == "SuspendedEVSE") return 3;
    if (s == "SuspendedEV")   return 4;
    if (s == "Finishing")     return 5;
    if (s == "Reserved")      return 6;
    if (s == "Unavailable")   return 7;
    if (s == "Faulted")       return 8;
    return 99;
}

CPEmulator::ConnectorState* CPEmulator::find_connector(Station& station, int connector_id)
{
    for (auto& cs : station.connector_states)
        if (cs.connector_id == connector_id) return &cs;
    return nullptr;
}

CPEmulator::ConnectorState* CPEmulator::find_connector_by_transaction(Station& station, int transaction_id)
{
    for (auto& cs : station.connector_states)
        if (cs.transaction_id == transaction_id) return &cs;
    return nullptr;
}

CPEmulator::IdTagInfo CPEmulator::parse_id_tag_info(const nlohmann::json& payload)
{
    IdTagInfo info;
    if (payload.contains("idTagInfo")) {
        auto& iti = payload["idTagInfo"];
        info.status = iti.value("status", "Invalid");
        info.parent_id_tag = iti.value("parentIdTag", "");
        if (iti.contains("expiryDate"))
            info.expiry_date = parse_iso_time(iti["expiryDate"].get<std::string>());
    }
    return info;
}

CPEmulator::IdTagInfo CPEmulator::authorize_local(Station& station, const std::string& id_tag)
{
    IdTagInfo info;
    auto& keys = station.config_keys;

    auto authorize_remote_it = keys.find("AuthorizeRemoteTxRequests");
    bool authorize_remote = (authorize_remote_it != keys.end() &&
                             authorize_remote_it->second.value == "true");

    if (authorize_remote) {
        auto cache_enabled_it = keys.find("AuthorizationCacheEnabled");
        bool cache_enabled = (cache_enabled_it != keys.end() &&
                              cache_enabled_it->second.value == "true");
        if (cache_enabled) {
            auto it = station.auth_cache.find(id_tag);
            if (it != station.auth_cache.end()) {
                info = it->second;
                if (info.expiry_date != std::chrono::system_clock::time_point{} &&
                    info.expiry_date <= std::chrono::system_clock::now()) {
                    info.status = "Expired";
                }
                return info;
            }
        }
        // Not in cache — status remains "Invalid", caller must send Authorize
    } else {
        // No remote authorization required — accept immediately
        info.status = "Accepted";
    }

    return info;
}

void CPEmulator::update_auth_cache(Station& station, const std::string& id_tag, const IdTagInfo& info)
{
    auto cache_enabled_it = station.config_keys.find("AuthorizationCacheEnabled");
    bool cache_enabled = (cache_enabled_it != station.config_keys.end() &&
                          cache_enabled_it->second.value == "true");
    if (cache_enabled) {
        station.auth_cache[id_tag] = info;
    }
}

void CPEmulator::send_status_notification(Station& station, int connector_id, const std::string& status)
{
    send_call(station, "StatusNotification", {
        {"connectorId", connector_id},
        {"errorCode", "NoError"},
        {"status", status},
        {"timestamp", iso_time_now()}
    });
}

void CPEmulator::continue_remote_start(Station& station, ConnectorState& conn)
{
    if (conn.status == "Available" || conn.status == "Reserved") {
        conn.status = "Preparing";
        conn.status_updated = std::chrono::system_clock::now();
        send_status_notification(station, conn.connector_id, "Preparing");
    }
    send_start_transaction(station, conn);
}

void CPEmulator::send_start_transaction(Station& station, ConnectorState& conn)
{
    nlohmann::json payload = {
        {"connectorId", conn.connector_id},
        {"idTag",       conn.id_tag},
        {"meterStart",  conn.meter_value},
        {"timestamp",   iso_time_now()}
    };

    if (conn.reservation_id >= 0)
        payload["reservationId"] = conn.reservation_id;

    send_call(station, "StartTransaction", std::move(payload),
              conn.connector_id, conn.id_tag);
}

void CPEmulator::local_stop_transaction(Station& station, ConnectorState& conn, const std::string& reason)
{
    nlohmann::json payload = {
        {"idTag",         conn.id_tag},
        {"meterStop",     conn.meter_value},
        {"timestamp",     iso_time_now()},
        {"transactionId", conn.transaction_id}
    };

    if (!reason.empty())
        payload["reason"] = reason;

    send_call(station, "StopTransaction", std::move(payload),
              conn.connector_id, conn.id_tag);

    conn.transaction_id = -1;
}

void CPEmulator::send_meter_values(Station& station, ConnectorState& conn)
{
    nlohmann::json payload = {
        {"connectorId", conn.connector_id},
        {"meterValue", {{
            {"timestamp", iso_time_now()},
            {"sampledValue", {{
                {"unit", "Wh"},
                {"value", std::to_string(conn.meter_value)},
                {"measurand", "Energy.Active.Import.Register"}
            }}}
        }}}
    };

    if (conn.transaction_id > 0)
        payload["transactionId"] = conn.transaction_id;

    send_call(station, "MeterValues", std::move(payload));
}

// ── CS→CP Action handlers ────────────────────────────────────────────────────

nlohmann::json CPEmulator::on_cancel_reservation(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("reservationId"))
        throw std::invalid_argument("missing reservationId");

    int reservation_id = payload["reservationId"].get<int>();

    // Find connector with this reservation
    ConnectorState* conn = nullptr;
    for (auto& cs : station.connector_states) {
        if (cs.reservation_id == reservation_id) {
            conn = &cs;
            break;
        }
    }

    if (!conn)
        return {{"status", "Rejected"}};

    conn->reservation_id = -1;
    conn->reservation_id_tag.clear();
    conn->reservation_expiry = {};
    conn->status = "Available";
    conn->status_updated = std::chrono::system_clock::now();
    send_status_notification(station, conn->connector_id, "Available");

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_change_availability(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "ChangeAvailability");
    return file.is_null() ? nlohmann::json{{"status", "Accepted"}} : file;
}

nlohmann::json CPEmulator::on_change_configuration(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("key") || !payload.contains("value"))
        throw std::invalid_argument("missing key or value");

    auto key = payload["key"].get<std::string>();
    auto value = payload["value"].get<std::string>();

    auto it = station.config_keys.find(key);
    if (it != station.config_keys.end() && it->second.readonly)
        return {{"status", "Rejected"}};

    station.config_keys[key] = {value, false};

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_clear_cache(Station& /*station*/, const nlohmann::json& /*payload*/)
{
    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_clear_charging_profile(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "ClearChargingProfile");
    return file.is_null() ? nlohmann::json{{"status", "Accepted"}} : file;
}

nlohmann::json CPEmulator::on_data_transfer(Station& station, const nlohmann::json& payload)
{
    // Handle vendor-specific messageId dispatching (v1 parity)
    if (payload.contains("messageId")) {
        auto message_id = payload.value("messageId", "");

        if (message_id == "SetStatusNotification" && payload.contains("data")) {
            try {
                auto data = nlohmann::json::parse(payload["data"].get<std::string>());
                if (data.contains("connectorId") && data.contains("status")) {
                    int connector_id = data["connectorId"].get<int>();
                    auto status = data["status"].get<std::string>();
                    send_call(station, "StatusNotification", {
                        {"connectorId", connector_id},
                        {"errorCode", "NoError"},
                        {"status", status}
                    });
                    return {{"status", "Accepted"}};
                }
            } catch (...) {}
            return {{"status", "Rejected"}};
        }

        return {{"status", "UnknownMessageId"}};
    }

    nlohmann::json response = {{"status", "Accepted"}};
    if (payload.contains("data"))
        response["data"] = payload["data"];
    return response;
}

nlohmann::json CPEmulator::on_get_composite_schedule(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "GetCompositeSchedule");
    return file.is_null() ? nlohmann::json{{"status", "Accepted"}} : file;
}

nlohmann::json CPEmulator::on_get_configuration(Station& station, const nlohmann::json& payload)
{
    nlohmann::json keys = nlohmann::json::array();
    for (const auto& [k, cv] : station.config_keys)
        keys.push_back({{"key", k}, {"value", cv.value}, {"readonly", cv.readonly}});

    return {{"configurationKey", keys}};
}

nlohmann::json CPEmulator::on_get_diagnostics(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "GetDiagnostics");
    return file.is_null() ? nlohmann::json{{"fileName", ""}} : file;
}

nlohmann::json CPEmulator::on_get_local_list_version(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "GetLocalListVersion");
    return file.is_null() ? nlohmann::json{{"listVersion", 0}} : file;
}

nlohmann::json CPEmulator::on_remote_start_transaction(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("idTag"))
        throw std::invalid_argument("missing idTag");

    auto id_tag = payload["idTag"].get<std::string>();
    int connector_id = payload.value("connectorId", 0);

    // Find target connector
    ConnectorState* conn = nullptr;
    if (connector_id > 0) {
        conn = find_connector(station, connector_id);
        if (!conn)
            throw std::invalid_argument(fmt::format("invalid connectorId: {}", connector_id));
    } else {
        // Find first available connector
        for (auto& cs : station.connector_states) {
            if (cs.status == "Available") {
                conn = &cs;
                break;
            }
        }
        if (!conn)
            return {{"status", "Rejected"}};
    }

    // Reject if connector is busy (Charging/Suspended/Finishing)
    // Reserved is handled separately below
    if (conn->status != "Reserved" && status_order(conn->status) > status_order("Preparing"))
        return {{"status", "Rejected"}};

    // Reject if reserved for another idTag
    if (conn->status == "Reserved" && conn->reservation_id_tag != id_tag)
        return {{"status", "Rejected"}};

    // Reject if already has active transaction
    if (conn->transaction_id >= 0)
        return {{"status", "Rejected"}};

    conn->id_tag = id_tag;

    // Authorize locally
    auto info = authorize_local(station, id_tag);

    if (info.status == "Accepted") {
        continue_remote_start(station, *conn);
    } else if (info.status == "Invalid") {
        // Not in cache, need to ask CS
        send_call(station, "Authorize", {{"idTag", id_tag}},
                  conn->connector_id, id_tag);
    } else {
        // Blocked/Expired/ConcurrentTx — reject
        return {{"status", "Rejected"}};
    }

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_remote_stop_transaction(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("transactionId"))
        throw std::invalid_argument("missing transactionId");

    int transaction_id = payload["transactionId"].get<int>();

    auto* conn = find_connector_by_transaction(station, transaction_id);
    if (!conn)
        return {{"status", "Rejected"}};

    // Only stop if Charging or Suspended
    int order = status_order(conn->status);
    if (order < status_order("Charging") || order > status_order("SuspendedEV"))
        return {{"status", "Rejected"}};

    // Reservation check
    if (conn->reservation_id > 0 && conn->id_tag != conn->reservation_id_tag)
        return {{"status", "Rejected"}};

    local_stop_transaction(station, *conn, "Remote");

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_reserve_now(Station& station, const nlohmann::json& payload)
{
    for (auto field : {"connectorId", "expiryDate", "idTag", "reservationId"}) {
        if (!payload.contains(field))
            throw std::invalid_argument(fmt::format("missing {}", field));
    }

    int connector_id = payload["connectorId"].get<int>();
    int reservation_id = payload["reservationId"].get<int>();
    auto id_tag = payload["idTag"].get<std::string>();
    auto expiry = parse_iso_time(payload["expiryDate"].get<std::string>());

    auto* conn = find_connector(station, connector_id);
    if (!conn)
        throw std::invalid_argument(fmt::format("invalid connectorId: {}", connector_id));

    if (reservation_id <= 0 || conn->status == "Faulted")
        return {{"status", "Faulted"}};

    if (conn->status == "Unavailable")
        return {{"status", "Unavailable"}};

    if (reservation_id == conn->reservation_id) {
        if (conn->status != "Reserved" || conn->reservation_id_tag != id_tag)
            return {{"status", "Occupied"}};
    } else {
        if (conn->status != "Available")
            return {{"status", "Occupied"}};
        conn->reservation_id = reservation_id;
        conn->reservation_id_tag = id_tag;
    }

    conn->reservation_expiry = expiry;
    conn->status = "Reserved";
    conn->status_updated = std::chrono::system_clock::now();
    send_status_notification(station, connector_id, "Reserved");

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_reset(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("type"))
        throw std::invalid_argument("missing type");

    app_->logger().info("[{}] Reset requested: {}", station.identity,
                        payload["type"].get<std::string>());
    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_send_local_list(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "SendLocalList");
    return file.is_null() ? nlohmann::json{{"status", "Accepted"}} : file;
}

nlohmann::json CPEmulator::on_set_charging_profile(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "SetChargingProfile");
    return file.is_null() ? nlohmann::json{{"status", "Accepted"}} : file;
}

nlohmann::json CPEmulator::on_trigger_message(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("requestedMessage"))
        throw std::invalid_argument("missing requestedMessage");

    auto requested = payload["requestedMessage"].get<std::string>();
    int connector_id = payload.value("connectorId", 0);

    // Trigger the requested message (OCPP 1.6 §5.13 — 6 message types)
    if (requested == "BootNotification") {
        send_boot_notification(station);
    } else if (requested == "Heartbeat") {
        send_heartbeat(station);
    } else if (requested == "StatusNotification") {
        send_status_notification(station, connector_id);
    } else if (requested == "DiagnosticsStatusNotification") {
        send_call(station, "DiagnosticsStatusNotification", {{"status", "Idle"}});
    } else if (requested == "FirmwareStatusNotification") {
        send_call(station, "FirmwareStatusNotification", {{"status", "Idle"}});
    } else if (requested == "MeterValues") {
        // Send real MeterValues for specified connector or first charging one
        ConnectorState* target = nullptr;
        if (connector_id > 0)
            target = find_connector(station, connector_id);
        else {
            for (auto& cs : station.connector_states) {
                if (cs.status == "Charging" || cs.status == "SuspendedEVSE" || cs.status == "SuspendedEV") {
                    target = &cs;
                    break;
                }
            }
            if (!target && !station.connector_states.empty())
                target = &station.connector_states[0];
        }
        if (target)
            send_meter_values(station, *target);
        else
            return {{"status", "Rejected"}};
    } else {
        return {{"status", "NotImplemented"}};
    }

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_unlock_connector(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "UnlockConnector");
    return file.is_null() ? nlohmann::json{{"status", "Unlocked"}} : file;
}

nlohmann::json CPEmulator::on_update_firmware(Station& station, const nlohmann::json& payload)
{
    auto file = load_response_file(station.prefix, "UpdateFirmware");
    return file.is_null() ? nlohmann::json::object() : file;
}

} // namespace apostol

#endif // WITH_POSTGRESQL
