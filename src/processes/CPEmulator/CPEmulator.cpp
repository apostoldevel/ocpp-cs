#ifdef WITH_POSTGRESQL

#include "CPEmulator.hpp"
#include "apostol/application.hpp"
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

        // Send BootNotification once after connect
        if (!station->boot_sent) {
            send_boot_notification(*station);
            station->boot_sent = true;
        }

        // Periodic heartbeat
        if (now >= station->next_heartbeat) {
            send_heartbeat(*station);
            station->next_heartbeat = now +
                std::chrono::seconds(station->heartbeat_interval);
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

    // Parse configuration keys
    std::unordered_map<std::string, std::string> config_keys;
    if (config.contains("configurationKey") && config["configurationKey"].is_array()) {
        for (const auto& entry : config["configurationKey"]) {
            if (entry.contains("key") && entry.contains("value"))
                config_keys[entry["key"].get<std::string>()] = entry["value"].get<std::string>();
        }
    }

    auto url_it = config_keys.find("CentralSystemURL");
    if (url_it == config_keys.end() || url_it->second.empty())
        throw std::runtime_error(fmt::format("[{}] CentralSystemURL not found in configuration", dir_name));

    auto identity_it = config_keys.find("StationIdentity");
    std::string identity = identity_it != config_keys.end() ? identity_it->second : dir_name;

    app_->logger().info("[{}] configuration: {}", identity, config_file);

    // Create station
    auto station = std::make_unique<Station>();
    station->identity     = identity;
    station->prefix       = station_dir;
    station->config       = std::move(config);
    station->connector_ids = std::move(connector_ids);
    station->config_keys  = std::move(config_keys);

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
    auto ws_url = url_it->second;
    if (!ws_url.empty() && ws_url.back() != '/')
        ws_url += '/';
    ws_url += identity;

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
    station.boot_sent = false;
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
    station.boot_sent = false;
    station.pending_calls.clear();
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

    auto action = std::move(it->second);
    station.pending_calls.erase(it);

    // Handle BootNotification response
    if (action == "BootNotification") {
        auto status = msg.payload.value("status", "Rejected");
        auto interval = msg.payload.value("interval", 60);
        app_->logger().info("[{}] BootNotification: status={} interval={}",
                            station.identity, status, interval);
        station.heartbeat_interval = interval;
        station.next_heartbeat = std::chrono::system_clock::now() +
            std::chrono::seconds(interval);

        // Send initial StatusNotification for all connectors
        if (status == "Accepted") {
            for (int cid : station.connector_ids)
                send_status_notification(station, cid);
        }
    }
}

void CPEmulator::handle_call_error(Station& station, const ocpp::OcppMessage& msg)
{
    auto it = station.pending_calls.find(msg.unique_id);
    std::string action = "unknown";
    if (it != station.pending_calls.end()) {
        action = std::move(it->second);
        station.pending_calls.erase(it);
    }

    app_->logger().warn("[{}] CallError for {}: {} - {}",
        station.identity, action, msg.error_code, msg.error_description);
}

// ── Sending helpers ──────────────────────────────────────────────────────────

void CPEmulator::send_call(Station& station, std::string_view action, nlohmann::json payload)
{
    auto msg = ocpp::make_call(action, std::move(payload));
    station.pending_calls[msg.unique_id] = std::string(action);
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

    // Try to load from file, fallback to defaults from config
    auto file_payload = load_response_file(station.prefix, "BootNotification");
    if (!file_payload.is_null()) {
        payload = std::move(file_payload);
    } else {
        payload = {
            {"chargePointVendor", station.config.value("chargePointVendor", "Test")},
            {"chargePointModel", station.config.value("chargePointModel", "TestModel")},
            {"chargePointSerialNumber", station.config.value("chargePointSerialNumber", "")},
            {"firmwareVersion", station.config.value("firmwareVersion", "1.0")}
        };
    }

    app_->logger().info("[{}] sending BootNotification", station.identity);
    send_call(station, "BootNotification", std::move(payload));
}

void CPEmulator::send_status_notification(Station& station, int connector_id)
{
    send_call(station, "StatusNotification", {
        {"connectorId", connector_id},
        {"errorCode", "NoError"},
        {"status", "Available"}
    });
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

// ── CS→CP Action handlers ────────────────────────────────────────────────────

nlohmann::json CPEmulator::on_cancel_reservation(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("reservationId"))
        throw std::invalid_argument("missing reservationId");
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
    station.config_keys[key] = value;

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
    for (const auto& [k, v] : station.config_keys)
        keys.push_back({{"key", k}, {"value", v}, {"readonly", false}});

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
    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_remote_stop_transaction(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("transactionId"))
        throw std::invalid_argument("missing transactionId");
    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_reserve_now(Station& station, const nlohmann::json& payload)
{
    for (auto field : {"connectorId", "expiryDate", "idTag", "reservationId"}) {
        if (!payload.contains(field))
            throw std::invalid_argument(fmt::format("missing {}", field));
    }
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

    // Trigger the requested message
    if (requested == "BootNotification") {
        send_boot_notification(station);
    } else if (requested == "Heartbeat") {
        send_heartbeat(station);
    } else if (requested == "StatusNotification") {
        send_status_notification(station, connector_id);
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
