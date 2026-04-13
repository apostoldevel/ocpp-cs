#ifdef WITH_POSTGRESQL

#include "CPEmulator.hpp"
#include "apostol/application.hpp"
#include "apostol/http_utils.hpp"
#include "ocpp/ocpp_codec.hpp"
#include "ocpp/time_utils.hpp"
#include "apostol/logger.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <chrono>
#include <unordered_set>

namespace apostol
{

namespace fs = std::filesystem;

static constexpr int    kMeterIncrementWh    = 10;   // simulated meter increment per tick
static constexpr auto   kMeterValuesInterval = std::chrono::seconds(60);
static constexpr auto   kReconnectDelay      = std::chrono::seconds(30);
static constexpr auto   kPingInterval        = std::chrono::seconds(30);

// ── Lifecycle ────────────────────────────────────────────────────────────────

void CPEmulator::on_start(EventLoop& loop, Application& app)
{
    loop_ = &loop;
    app_  = &app;

    prefix_ = app.settings().prefix;
    if (!prefix_.empty() && prefix_.back() != '/')
        prefix_ += '/';

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
                if (station->ocpp_version == "2.0.1")
                    send_boot_notification_201(*station);
                else
                    send_boot_notification(*station);
                station->boot_status = BootStatus::pending;
                station->next_boot_retry = {};  // will be set by response handler
            }
            continue;  // no heartbeat until boot Accepted
        }

        // ── OCPP 2.0.1: per-EVSE logic ─────────────────────────────────

        if (station->ocpp_version == "2.0.1") {
            // 2.0.1: per-EVSE meter increment
            for (auto& evse : station->evses) {
                if (!evse.transaction_id.empty() && evse.status == "Occupied")
                    evse.meter_value += kMeterIncrementWh;
            }

            // Cancel expired reservations (H04)
            for (auto& evse : station->evses) {
                if (evse.reservation && evse.reservation->expiry != std::chrono::system_clock::time_point{} &&
                    now >= evse.reservation->expiry) {
                    int res_id = evse.reservation->id;
                    app_->logger().info("[{}] reservation {} expired on EVSE {}",
                        station->identity, res_id, evse.evse_id);

                    evse.reservation.reset();
                    evse.status = "Available";
                    evse.status_updated = now;

                    for (auto& conn : evse.connectors)
                        send_status_notification_201(*station, evse.evse_id, conn.connector_id);

                    send_reservation_status_update(*station, res_id, "Expired");
                }
            }

            // Periodic TransactionEvent(Updated) with MeterValues
            bool meter_sent = false;
            if (now >= station->next_meter_values) {
                station->next_meter_values = now + kMeterValuesInterval;
                for (auto& evse : station->evses) {
                    if (!evse.transaction_id.empty() && evse.status == "Occupied") {
                        send_transaction_event(*station, evse, "Updated", "MeterValuePeriodic");
                        meter_sent = true;
                    }
                }
            }

            // Heartbeat (only if no meter values sent this tick)
            if (!meter_sent && now >= station->next_heartbeat) {
                send_heartbeat_201(*station);
                station->next_heartbeat = now +
                    std::chrono::seconds(station->heartbeat_interval);
            }
            continue;  // skip 1.6 logic below
        }

        // ── Per-connector logic (v1 parity) ───────────────────────────────

        for (auto& conn : station->connector_states) {
            if (conn.status == "Charging") {
                conn.meter_value += kMeterIncrementWh;
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
            station->next_meter_values = now + kMeterValuesInterval;
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

    // Parse 2.0.1 style configuration keys (object format)
    if (config.contains("ConfigurationKeys") && config["ConfigurationKeys"].is_object()) {
        for (auto& [key, val] : config["ConfigurationKeys"].items()) {
            if (val.is_object() && val.contains("value"))
                config_keys[key] = {val["value"].get<std::string>(), val.value("readonly", false)};
        }
    }

    // Top-level CentralSystemURL (used by 2.0.1 configs)
    if (config.contains("CentralSystemURL") && !config["CentralSystemURL"].is_null()) {
        auto url = config["CentralSystemURL"].get<std::string>();
        if (!url.empty())
            config_keys["CentralSystemURL"] = {url, true};
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

    // Detect OCPP version
    station->ocpp_version = station->config.value("OcppVersion", "1.6");

    if (station->ocpp_version == "2.0.1") {
        // Parse EVSE/Connector hierarchy from config
        if (station->config.contains("Evses")) {
            for (const auto& evse_json : station->config["Evses"]) {
                EvseState evse;
                evse.evse_id = evse_json["evseId"].get<int>();
                if (evse_json.contains("connectors")) {
                    for (const auto& conn_json : evse_json["connectors"]) {
                        ConnectorState201 conn;
                        conn.connector_id = conn_json["connectorId"].get<int>();
                        conn.connector_type = conn_json.value("type", "cCCS2");
                        evse.connectors.push_back(std::move(conn));
                    }
                }
                station->evses.push_back(std::move(evse));
            }
        }

        // Initialize minimal Device Model
        auto& dm = station->device_model;
        dm.push_back({"ChargingStation", "Vendor",
                      station->config.value("ChargePointVendor", "Emulator"), true, "ReadOnly", 0, "string"});
        dm.push_back({"ChargingStation", "Model",
                      station->config.value("ChargePointModel", "Virtual-201"), true, "ReadOnly", 0, "string"});
        dm.push_back({"ChargingStation", "SerialNumber",
                      station->config.value("ChargePointSerialNumber", "EM-201-001"), true, "ReadOnly", 0, "string"});
        dm.push_back({"ChargingStation", "FirmwareVersion",
                      station->config.value("ChargePointSoftwareVersion", "2.0.0"), true, "ReadOnly", 0, "string"});
        dm.push_back({"OCPPCommCtrl", "HeartbeatInterval",
                      std::to_string(station->heartbeat_interval), false, "ReadWrite", 0, "integer"});
        dm.push_back({"OCPPCommCtrl", "NetworkProfileUri",
                      station->config.value("CentralSystemURL", ""), true, "ReadOnly", 0, "string"});
        dm.push_back({"AuthCtrl", "LocalAuthListEnabled", "true", false, "ReadWrite", 0, "boolean"});
        dm.push_back({"AuthCtrl", "AuthCacheEnabled", "true", false, "ReadWrite", 0, "boolean"});
        dm.push_back({"ReservationCtrlr", "NonEvseSpecific", "true", false, "ReadWrite", 0, "boolean"});
        for (auto& evse : station->evses) {
            dm.push_back({"EVSE", "Power", "22000", true, "ReadOnly", evse.evse_id, "integer"});
            dm.push_back({"EVSE", "AvailabilityState", evse.status, true, "ReadOnly", evse.evse_id, "string"});
        }
    } else {
        // Initialize per-connector state (1.6)
        for (int cid : connector_ids)
            station->connector_states.push_back(ConnectorState{.connector_id = cid, .id_tag = {}, .reservation_id_tag = {}});
    }

    // Create WsClient with OcppCodec
    station->ws = std::make_unique<WsClient>(*loop_);
    station->ws->set_codec(std::make_unique<ocpp::OcppCodec>());
    station->ws->auto_reconnect(true);
    station->ws->set_reconnect_max_delay(kReconnectDelay);
    station->ws->set_ping_interval(kPingInterval);

    auto* st = station.get();

    station->ws->on_connect([this, st] {
        on_ws_connect(*st);
    });

    station->ws->on_close([this, st](uint16_t code, std::string_view reason) {
        on_ws_close(*st, code, reason);
    });

    station->ws->on_error([this, st](std::string_view err) {
        app_->logger().error("[{}] WS error: {}", st->identity, err);
    });

    // Register OCPP action handlers (CS→CP) via WsClient RPC layer
    register_action_handlers(*station);

    // Build URL: ws://host/path/identity
    auto ws_url = url_it->second.value;
    if (!ws_url.empty() && ws_url.back() != '/')
        ws_url += '/';
    ws_url += url_encode(identity);

    app_->logger().info("[{}] connecting to {} (OCPP {})", identity, ws_url, station->ocpp_version);

    if (station->ocpp_version == "2.0.1")
        station->ws->connect(ws_url, {"ocpp2.0.1"});
    else
        station->ws->connect(ws_url, {"ocpp1.6"});

    stations_.push_back(std::move(station));
}

// ── Action handler registration (CS→CP) ─────────────────────────────────────

void CPEmulator::register_action_handlers(Station& station)
{
    // Helper: wraps an on_* method into WsClient::ActionHandler.
    // Catches exceptions and converts them to CallError responses.
    auto reg = [this, &station](std::string_view action,
                                nlohmann::json(CPEmulator::*fn)(Station&, const nlohmann::json&)) {
        station.ws->on_action(action,
            [this, &station, fn](WsClient&, const WsMessage& req, WsMessage& resp) {
                try {
                    resp.type = WsMessage::Type::Response;
                    resp.payload = (this->*fn)(station, req.payload);
                } catch (const std::exception& e) {
                    resp.type = WsMessage::Type::Error;
                    resp.error_code = "InternalError";
                    resp.error_description = e.what();
                }
            });
    };

    if (station.ocpp_version == "2.0.1") {
        reg("RequestStartTransaction", &CPEmulator::on_request_start_transaction);
        reg("RequestStopTransaction",  &CPEmulator::on_request_stop_transaction);
        reg("Reset",                   &CPEmulator::on_reset_201);
        reg("SetVariables",            &CPEmulator::on_set_variables);
        reg("GetVariables",            &CPEmulator::on_get_variables);
        reg("ChangeAvailability",      &CPEmulator::on_change_availability_201);
        reg("DataTransfer",            &CPEmulator::on_data_transfer_201);
        // Phase 2: Provisioning
        reg("GetBaseReport",        &CPEmulator::on_get_base_report);
        reg("GetReport",            &CPEmulator::on_get_report);
        // Phase 2: Availability
        reg("UnlockConnector",      &CPEmulator::on_unlock_connector_201);
        reg("TriggerMessage",       &CPEmulator::on_trigger_message_201);
        reg("ClearCache",           &CPEmulator::on_clear_cache_201);
        reg("GetTransactionStatus", &CPEmulator::on_get_transaction_status);
        // Phase 2: Local Auth List
        reg("SendLocalList",        &CPEmulator::on_send_local_list_201);
        reg("GetLocalListVersion",  &CPEmulator::on_get_local_list_version_201);
        // Phase 2: Reservation
        reg("ReserveNow",        &CPEmulator::on_reserve_now_201);
        reg("CancelReservation", &CPEmulator::on_cancel_reservation_201);
    } else {
        reg("CancelReservation",      &CPEmulator::on_cancel_reservation);
        reg("ChangeAvailability",     &CPEmulator::on_change_availability);
        reg("ChangeConfiguration",    &CPEmulator::on_change_configuration);
        reg("ClearCache",             &CPEmulator::on_clear_cache);
        reg("ClearChargingProfile",   &CPEmulator::on_clear_charging_profile);
        reg("DataTransfer",           &CPEmulator::on_data_transfer);
        reg("GetCompositeSchedule",   &CPEmulator::on_get_composite_schedule);
        reg("GetConfiguration",       &CPEmulator::on_get_configuration);
        reg("GetDiagnostics",         &CPEmulator::on_get_diagnostics);
        reg("GetLocalListVersion",    &CPEmulator::on_get_local_list_version);
        reg("RemoteStartTransaction", &CPEmulator::on_remote_start_transaction);
        reg("RemoteStopTransaction",  &CPEmulator::on_remote_stop_transaction);
        reg("ReserveNow",             &CPEmulator::on_reserve_now);
        reg("Reset",                  &CPEmulator::on_reset);
        reg("SendLocalList",          &CPEmulator::on_send_local_list);
        reg("SetChargingProfile",     &CPEmulator::on_set_charging_profile);
        reg("TriggerMessage",         &CPEmulator::on_trigger_message);
        reg("UnlockConnector",        &CPEmulator::on_unlock_connector);
        reg("UpdateFirmware",         &CPEmulator::on_update_firmware);
    }
}

// ── WebSocket callbacks ──────────────────────────────────────────────────────

void CPEmulator::on_ws_connect(Station& station)
{
    app_->logger().info("[{}] connected (OCPP {})", station.identity, station.ocpp_version);
    station.boot_status = BootStatus::not_sent;
    station.next_boot_retry = {};
}

void CPEmulator::on_ws_close(Station& station, uint16_t code, std::string_view reason)
{
    app_->logger().info("[{}] disconnected: {} {}", station.identity, code, reason);
    station.boot_status = BootStatus::not_sent;
    station.next_boot_retry = {};
    station.next_meter_values = {};

    // Reset all connectors on disconnect (1.6)
    for (auto& conn : station.connector_states)
        conn.reset();

    // Reset all EVSEs on disconnect (2.0.1)
    for (auto& evse : station.evses)
        evse.reset();
}

// ── Sending helper ───────────────────────────────────────────────────────────

void CPEmulator::send_request(Station& station, std::string_view action,
                               nlohmann::json payload, WsClient::ResponseHandler on_response)
{
    WsMessage msg;
    msg.type    = WsMessage::Type::Request;
    msg.action  = std::string(action);
    msg.payload = std::move(payload);

    station.ws->send(msg, std::move(on_response));
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

        // Send geo coordinates as extra fields (OCPP 1.6 has no strict schema enforcement)
        if (station.config.contains("Geo")) {
            auto& geo = station.config["Geo"];
            if (geo.contains("latitude"))  payload["latitude"]  = geo["latitude"];
            if (geo.contains("longitude")) payload["longitude"] = geo["longitude"];
            if (geo.contains("location"))  payload["location"]  = geo["location"];
        }
    }

    app_->logger().info("[{}] sending BootNotification", station.identity);

    send_request(station, "BootNotification", std::move(payload),
        [this, identity = station.identity](const WsMessage& resp) {
            auto* st = find_station(identity);
            if (!st) return;

            if (resp.type == WsMessage::Type::Error) {
                app_->logger().warn("[{}] BootNotification error: {} - {}",
                    identity, resp.error_code, resp.error_description);
                return;
            }

            auto status = resp.payload.value("status", "Rejected");
            auto interval = resp.payload.value("interval", 60);
            auto now = std::chrono::system_clock::now();

            app_->logger().info("[{}] BootNotification: status={} interval={}",
                                identity, status, interval);
            st->heartbeat_interval = interval;

            if (status == "Accepted") {
                st->boot_status = BootStatus::accepted;
                st->next_heartbeat = now + std::chrono::seconds(interval);
                send_status_notification(*st, 0);
                for (auto& conn : st->connector_states)
                    send_status_notification(*st, conn.connector_id, conn.status);
            } else {
                st->boot_status = (status == "Pending")
                    ? BootStatus::pending : BootStatus::rejected;
                st->next_boot_retry = now + std::chrono::seconds(interval);
            }
        });
}

void CPEmulator::send_status_notification(Station& station, int connector_id)
{
    std::string status = "Available";
    std::string error_code = "NoError";
    auto* conn = find_connector(station, connector_id);
    if (conn) {
        status = conn->status;
        error_code = conn->error_code;
    }

    send_status_notification(station, connector_id, status, error_code);
}

void CPEmulator::send_heartbeat(Station& station)
{
    send_request(station, "Heartbeat", nlohmann::json::object());
}

// ── OCPP 2.0.1 Boot sequence ────────────────────────────────────────────────

void CPEmulator::send_boot_notification_201(Station& station)
{
    auto get_key = [&](const char* k) -> std::string {
        auto it = station.config_keys.find(k);
        if (it != station.config_keys.end() && !it->second.value.empty())
            return it->second.value;
        return station.config.value(k, "");
    };

    nlohmann::json charging_station = {
        {"vendorName", get_key("ChargePointVendor")},
        {"model", get_key("ChargePointModel")}
    };

    auto serial = get_key("ChargePointSerialNumber");
    if (!serial.empty()) charging_station["serialNumber"] = serial;

    auto firmware = get_key("ChargePointSoftwareVersion");
    if (!firmware.empty()) charging_station["firmwareVersion"] = firmware;

    app_->logger().info("[{}] sending BootNotification (2.0.1)", station.identity);

    nlohmann::json boot_payload = {
        {"reason", "PowerUp"},
        {"chargingStation", charging_station}
    };

    // Send geo coordinates via customData (vendor extension, not part of OCPP 2.0.1 spec)
    if (station.config.contains("Geo")) {
        auto& geo = station.config["Geo"];
        boot_payload["customData"] = {
            {"vendorId", "ChargeMeCar"},
            {"latitude", geo.value("latitude", 0.0)},
            {"longitude", geo.value("longitude", 0.0)},
            {"location", geo.value("location", "")}
        };
    }

    send_request(station, "BootNotification", std::move(boot_payload),
        [this, identity = station.identity](const WsMessage& resp) {
        auto* st = find_station(identity);
        if (!st) return;

        if (resp.type == WsMessage::Type::Error) {
            app_->logger().warn("[{}] BootNotification error: {} - {}",
                identity, resp.error_code, resp.error_description);
            return;
        }

        auto status = resp.payload.value("status", "Rejected");
        auto interval = resp.payload.value("interval", 60);
        auto now = std::chrono::system_clock::now();

        app_->logger().info("[{}] BootNotification: status={} interval={}",
                            identity, status, interval);
        st->heartbeat_interval = interval;

        if (status == "Accepted") {
            st->boot_status = BootStatus::accepted;
            st->next_heartbeat = now + std::chrono::seconds(interval);

            // Send StatusNotification for each EVSE+Connector
            for (auto& evse : st->evses) {
                for (auto& conn : evse.connectors) {
                    send_status_notification_201(*st, evse.evse_id, conn.connector_id);
                }
            }
        } else {
            st->boot_status = (status == "Pending")
                ? BootStatus::pending : BootStatus::rejected;
            st->next_boot_retry = now + std::chrono::seconds(interval);
        }
    });
}

void CPEmulator::send_status_notification_201(Station& station, int evse_id, int connector_id)
{
    std::string status = "Available";
    auto* evse = find_evse(station, evse_id);
    if (evse)
        status = evse->status;

    send_request(station, "StatusNotification", {
        {"timestamp", ocpp::iso_time_now()},
        {"connectorStatus", status},
        {"evseId", evse_id},
        {"connectorId", connector_id}
    });
}

void CPEmulator::send_reservation_status_update(Station& station, int reservation_id,
                                                 const std::string& status)
{
    send_request(station, "ReservationStatusUpdate", {
        {"reservationId", reservation_id},
        {"reservationUpdateStatus", status}
    });
}

void CPEmulator::send_heartbeat_201(Station& station)
{
    send_request(station, "Heartbeat", nlohmann::json::object());
}

// ── OCPP 2.0.1 Transactions ────────────────────────────────────────────────

void CPEmulator::send_transaction_event(Station& station, EvseState& evse,
                                         const std::string& event_type,
                                         const std::string& trigger_reason)
{
    nlohmann::json payload = {
        {"eventType", event_type},
        {"timestamp", ocpp::iso_time_now()},
        {"triggerReason", trigger_reason},
        {"seqNo", evse.seq_no++},
        {"transactionInfo", {
            {"transactionId", evse.transaction_id}
        }}
    };

    // Add EVSE info (use active_connector_id from CSMS hint if available)
    nlohmann::json evse_info = {{"id", evse.evse_id}};
    if (evse.active_connector_id > 0)
        evse_info["connectorId"] = evse.active_connector_id;
    else if (!evse.connectors.empty())
        evse_info["connectorId"] = evse.connectors[0].connector_id;
    payload["evse"] = evse_info;

    // Add idToken for Started
    if (event_type == "Started" && !evse.id_token.empty()) {
        payload["idToken"] = {
            {"idToken", evse.id_token},
            {"type", evse.id_token_type}
        };
    }

    if (evse.remote_start_id > 0)
        payload["transactionInfo"]["remoteStartId"] = evse.remote_start_id;

    // Add meterValue for Updated and Ended
    if (event_type == "Updated" || event_type == "Ended") {
        payload["meterValue"] = nlohmann::json::array({
            {
                {"timestamp", ocpp::iso_time_now()},
                {"sampledValue", nlohmann::json::array({
                    {
                        {"value", evse.meter_value},
                        {"measurand", "Energy.Active.Import.Register"},
                        {"unitOfMeasure", {{"unit", "Wh"}}}
                    }
                })}
            }
        });
    }

    // Add stoppedReason for Ended
    if (event_type == "Ended") {
        auto stopped_reason = [&trigger_reason]() -> std::string {
            if (trigger_reason == "RemoteStop")    return "Remote";
            if (trigger_reason == "ResetCommand")  return "ImmediateReset";
            if (trigger_reason == "UnlockCommand") return "EVDisconnected";
            if (trigger_reason == "DeAuthorized")  return "DeAuthorized";
            if (trigger_reason == "Local")         return "Local";
            return "Other";
        }();
        payload["transactionInfo"]["stoppedReason"] = stopped_reason;
    }

    if (event_type == "Ended" && !evse.pending_availability.empty()) {
        auto pending = evse.pending_availability;
        evse.pending_availability.clear();
        auto eid = evse.evse_id;
        loop_->add_timer(std::chrono::milliseconds(50), [this, identity = station.identity, eid, pending]() {
            auto* st = find_station(identity);
            if (!st) return;
            auto* e = find_evse(*st, eid);
            if (e) {
                e->status = pending;
                auto* dm_var = find_device_model_var(*st, "EVSE", "AvailabilityState", eid);
                if (dm_var) dm_var->value = pending;
                for (auto& c : e->connectors)
                    send_status_notification_201(*st, e->evse_id, c.connector_id);
            }
        }, false);
    }

    WsClient::ResponseHandler handler;
    if (event_type == "Started") {
        handler = [this, identity = station.identity, eid = evse.evse_id](const WsMessage& resp) {
            if (resp.type == WsMessage::Type::Error) return;
            auto* st = find_station(identity);
            if (!st) return;
            auto* ev = find_evse(*st, eid);
            if (!ev) return;

            if (resp.payload.contains("idTokenInfo")) {
                auto status = resp.payload["idTokenInfo"].value("status", "Invalid");
                if (status == "Accepted") {
                    ev->status = "Occupied";
                    ev->status_updated = std::chrono::system_clock::now();
                    int cid = ev->active_connector_id > 0
                        ? ev->active_connector_id
                        : (!ev->connectors.empty() ? ev->connectors[0].connector_id : 1);
                    send_status_notification_201(*st, ev->evse_id, cid);
                }
            }
        };
    }

    send_request(station, "TransactionEvent", std::move(payload), std::move(handler));
}

void CPEmulator::send_authorize_201(Station& station, EvseState& evse)
{
    send_request(station, "Authorize", {
        {"idToken", {
            {"idToken", evse.id_token},
            {"type", evse.id_token_type}
        }}
    }, [this, identity = station.identity, eid = evse.evse_id](const WsMessage& resp) {
        if (resp.type == WsMessage::Type::Error) return;
        auto* st = find_station(identity);
        if (!st) return;
        auto* ev = find_evse(*st, eid);
        if (!ev) return;

        if (resp.payload.contains("idTokenInfo")) {
            auto status = resp.payload["idTokenInfo"].value("status", "Invalid");
            if (status == "Accepted") {
                // Generate transaction ID and start transaction
                ev->transaction_id = generate_transaction_id();
                ev->seq_no = 0;
                ev->status_updated = std::chrono::system_clock::now();
                send_transaction_event(*st, *ev, "Started", "RemoteStart");
            }
        }
    });
}

// ── OCPP 2.0.1 Helpers ──────────────────────────────────────────────────────

CPEmulator::EvseState* CPEmulator::find_evse(Station& station, int evse_id)
{
    for (auto& evse : station.evses)
        if (evse.evse_id == evse_id) return &evse;
    return nullptr;
}

CPEmulator::ReportVariable* CPEmulator::find_device_model_var(
    Station& station, const std::string& component,
    const std::string& variable, int evse_id)
{
    for (auto& v : station.device_model) {
        if (v.component == component && v.variable == variable && v.evse_id == evse_id)
            return &v;
    }
    return nullptr;
}

std::string CPEmulator::generate_transaction_id()
{
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    auto a = dist(gen), b = dist(gen);
    return fmt::format("{:08x}-{:04x}-4{:03x}-{:04x}-{:012x}",
        (a >> 32) & 0xFFFFFFFF,
        (a >> 16) & 0xFFFF,
        a & 0xFFF,
        0x8000 | ((b >> 48) & 0x3FFF),
        b & 0xFFFFFFFFFFFF);
}

// ── Station lookup ───────────────────────────────────────────────────────────

CPEmulator::Station* CPEmulator::find_station(const std::string& identity)
{
    for (auto& s : stations_)
        if (s->identity == identity) return s.get();
    return nullptr;
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

using ocpp::iso_time_now;
using ocpp::parse_iso_time;

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

void CPEmulator::send_status_notification(Station& station, int connector_id,
                                          const std::string& status, const std::string& error_code)
{
    send_request(station, "StatusNotification", {
        {"connectorId", connector_id},
        {"errorCode", error_code},
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

    int cid = conn.connector_id;
    std::string id_tag = conn.id_tag;

    send_request(station, "StartTransaction", std::move(payload),
        [this, identity = station.identity, cid, id_tag](const WsMessage& resp) {
            auto* st = find_station(identity);
            if (!st) return;

            if (resp.type == WsMessage::Type::Error) {
                app_->logger().warn("[{}] StartTransaction error: {} - {}",
                    identity, resp.error_code, resp.error_description);
                return;
            }

            auto* conn = find_connector(*st, cid);
            if (!conn) return;

            auto now = std::chrono::system_clock::now();

            // transactionId is required per OCPP 1.6 §6.46
            if (!resp.payload.contains("transactionId")) {
                app_->logger().error("[{}] StartTransaction.conf missing transactionId",
                    identity);
                return;
            }
            conn->transaction_id = resp.payload["transactionId"].get<int>();

            // idTagInfo is required per OCPP 1.6 §6.46
            IdTagInfo info;
            if (!resp.payload.contains("idTagInfo")) {
                app_->logger().warn("[{}] StartTransaction.conf missing idTagInfo — assuming Accepted",
                    identity);
                info.status = "Accepted";
            } else {
                info = parse_id_tag_info(resp.payload);
                update_auth_cache(*st, id_tag, info);
            }

            app_->logger().info("[{}] StartTransaction: transactionId={} connector={} auth={}",
                identity, conn->transaction_id, conn->connector_id, info.status);

            if (info.status == "Accepted") {
                // Determine charging status from config or default "Charging"
                std::string charge_status = "Charging";
                auto cfg_it = st->config_keys.find("StartTransactionStatus");
                if (cfg_it != st->config_keys.end() && !cfg_it->second.value.empty())
                    charge_status = cfg_it->second.value;

                conn->status = charge_status;
                conn->status_updated = now;
                send_status_notification(*st, conn->connector_id, charge_status);
            } else {
                // Authorization rejected — stop transaction immediately
                app_->logger().warn("[{}] StartTransaction auth rejected: {}",
                    identity, info.status);
                local_stop_transaction(*st, *conn, info.status);
            }
        });
}

// Valid Reason enum values per OCPP 1.6 §7.33
static bool is_valid_stop_reason(const std::string& reason)
{
    static const std::unordered_set<std::string> valid = {
        "DeAuthorized", "EmergencyStop", "EVDisconnected", "HardReset",
        "Local", "Other", "PowerLoss", "Reboot", "Remote",
        "SoftReset", "UnlockCommand"
    };
    return valid.count(reason) > 0;
}

void CPEmulator::local_stop_transaction(Station& station, ConnectorState& conn, const std::string& reason)
{
    nlohmann::json payload = {
        {"idTag",         conn.id_tag},
        {"meterStop",     conn.meter_value},
        {"timestamp",     iso_time_now()},
        {"transactionId", conn.transaction_id}
    };

    if (!reason.empty()) {
        // Map to valid OCPP reason or fall back to "Other"
        payload["reason"] = is_valid_stop_reason(reason) ? reason : "Other";
    }

    int cid = conn.connector_id;
    std::string id_tag = conn.id_tag;

    send_request(station, "StopTransaction", std::move(payload),
        [this, identity = station.identity, cid, id_tag](const WsMessage& resp) {
            auto* st = find_station(identity);
            if (!st) return;

            if (resp.type == WsMessage::Type::Error) {
                app_->logger().warn("[{}] StopTransaction error: {} - {}",
                    identity, resp.error_code, resp.error_description);
                return;
            }

            auto* conn = find_connector(*st, cid);
            if (!conn) return;

            if (resp.payload.contains("idTagInfo")) {
                auto info = parse_id_tag_info(resp.payload);
                update_auth_cache(*st, id_tag, info);
            }

            conn->status = "Finishing";
            conn->status_updated = std::chrono::system_clock::now();
            send_status_notification(*st, conn->connector_id, "Finishing");
        });

    conn.transaction_id = -1;
}

void CPEmulator::send_meter_values(Station& station, ConnectorState& conn)
{
    nlohmann::json payload = {
        {"connectorId", conn.connector_id},
        {"meterValue", nlohmann::json::array({
            {
                {"timestamp", iso_time_now()},
                {"sampledValue", nlohmann::json::array({
                    {
                        {"unit", "Wh"},
                        {"value", std::to_string(conn.meter_value)},
                        {"measurand", "Energy.Active.Import.Register"}
                    }
                })}
            }
        })}
    };

    if (conn.transaction_id > 0)
        payload["transactionId"] = conn.transaction_id;

    send_request(station, "MeterValues", std::move(payload));
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

nlohmann::json CPEmulator::on_reserve_now_201(Station& station, const nlohmann::json& payload)
{
    int reservation_id = payload.value("id", 0);
    if (reservation_id <= 0)
        throw std::invalid_argument("missing id");

    if (!payload.contains("expiryDateTime"))
        throw std::invalid_argument("missing expiryDateTime");
    if (!payload.contains("idToken"))
        throw std::invalid_argument("missing idToken");

    auto expiry = parse_iso_time(payload["expiryDateTime"].get<std::string>());
    auto id_token_obj = payload["idToken"];
    auto id_token = id_token_obj.value("idToken", "");
    auto id_token_type = id_token_obj.value("type", "ISO14443");

    std::string group_id_token;
    if (payload.contains("groupIdToken") && payload["groupIdToken"].is_object())
        group_id_token = payload["groupIdToken"].value("idToken", "");

    int evse_id = payload.value("evseId", 0);

    // Helper: apply reservation to an EVSE
    auto apply_reservation = [&](EvseState& evse) -> nlohmann::json {
        // H01.FR.02: same reservationId — replace
        if (evse.reservation && evse.reservation->id == reservation_id) {
            // Update in-place
        } else if (evse.status == "Faulted") {
            return {{"status", "Faulted"}};
        } else if (evse.status == "Unavailable") {
            return {{"status", "Unavailable"}};
        } else if (evse.status != "Available" && evse.status != "Reserved") {
            return {{"status", "Occupied"}};
        } else if (evse.status == "Reserved" &&
                   (!evse.reservation || evse.reservation->id != reservation_id)) {
            return {{"status", "Occupied"}};
        }

        evse.reservation = Reservation201{
            reservation_id, id_token, id_token_type,
            group_id_token, evse.evse_id, expiry
        };
        evse.status = "Reserved";
        evse.status_updated = std::chrono::system_clock::now();

        for (auto& conn : evse.connectors)
            send_status_notification_201(station, evse.evse_id, conn.connector_id);

        app_->logger().info("[{}] ReserveNow (2.0.1): id={} evse={} idToken={}",
            station.identity, reservation_id, evse.evse_id, id_token);

        return {{"status", "Accepted"}};
    };

    if (evse_id > 0) {
        // S2: specific EVSE
        auto* evse = find_evse(station, evse_id);
        if (!evse)
            return {{"status", "Rejected"}};
        return apply_reservation(*evse);
    }

    // S1: non-specific EVSE
    auto* var = find_device_model_var(station, "ReservationCtrlr", "NonEvseSpecific");
    if (!var || var->value != "true")
        return {{"status", "Rejected"}};

    // Find first available EVSE
    int faulted = 0, unavailable = 0;
    for (auto& evse : station.evses) {
        if (evse.status == "Available")
            return apply_reservation(evse);
        if (evse.status == "Faulted") faulted++;
        if (evse.status == "Unavailable") unavailable++;
    }

    if (faulted == static_cast<int>(station.evses.size()))
        return {{"status", "Faulted"}};
    if (unavailable == static_cast<int>(station.evses.size()))
        return {{"status", "Unavailable"}};

    return {{"status", "Occupied"}};
}

nlohmann::json CPEmulator::on_cancel_reservation_201(Station& station, const nlohmann::json& payload)
{
    int reservation_id = payload.value("reservationId", 0);
    if (reservation_id <= 0)
        throw std::invalid_argument("missing reservationId");

    for (auto& evse : station.evses) {
        if (evse.reservation && evse.reservation->id == reservation_id) {
            app_->logger().info("[{}] CancelReservation (2.0.1): id={} evse={}",
                station.identity, reservation_id, evse.evse_id);

            evse.reservation.reset();
            evse.status = "Available";
            evse.status_updated = std::chrono::system_clock::now();

            for (auto& conn : evse.connectors)
                send_status_notification_201(station, evse.evse_id, conn.connector_id);

            return {{"status", "Accepted"}};
        }
    }

    return {{"status", "Rejected"}};
}

nlohmann::json CPEmulator::on_change_availability(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("connectorId") || !payload.contains("type"))
        throw std::invalid_argument("missing connectorId or type");

    int connector_id = payload["connectorId"].get<int>();
    auto type = payload["type"].get<std::string>();

    // Determine target status from type (OCPP 1.6 §5.2)
    std::string target_status = (type == "Inoperative") ? "Unavailable" : "Available";
    auto now = std::chrono::system_clock::now();
    bool scheduled = false;

    auto apply = [&](ConnectorState& conn) {
        if (conn.transaction_id >= 0) {
            // Transaction in progress — schedule change for after completion
            scheduled = true;
            return;
        }
        if (conn.status != target_status) {
            conn.status = target_status;
            conn.error_code = "NoError";
            conn.status_updated = now;
            send_status_notification(station, conn.connector_id, target_status);
        }
    };

    if (connector_id == 0) {
        // Apply to all connectors
        for (auto& conn : station.connector_states)
            apply(conn);
    } else {
        auto* conn = find_connector(station, connector_id);
        if (!conn)
            throw std::invalid_argument(fmt::format("invalid connectorId: {}", connector_id));
        apply(*conn);
    }

    return {{"status", scheduled ? "Scheduled" : "Accepted"}};
}

nlohmann::json CPEmulator::on_change_configuration(Station& station, const nlohmann::json& payload)
{
    if (!payload.contains("key") || !payload.contains("value"))
        throw std::invalid_argument("missing key or value");

    auto key = payload["key"].get<std::string>();
    auto value = payload["value"].get<std::string>();

    auto it = station.config_keys.find(key);
    if (it == station.config_keys.end())
        return {{"status", "NotSupported"}};

    if (it->second.readonly)
        return {{"status", "Rejected"}};

    it->second.value = value;

    if (key == "CentralSystemURL" || key == "HeartbeatInterval")
        return {{"status", "RebootRequired"}};

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_clear_cache(Station& station, const nlohmann::json& /*payload*/)
{
    auto it = station.config_keys.find("AuthorizationCacheEnabled");
    if (it == station.config_keys.end() || it->second.value != "true")
        return {{"status", "Rejected"}};

    station.auth_cache.clear();
    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_clear_charging_profile(Station& station, const nlohmann::json& /*payload*/)
{
    auto file = load_response_file(station.prefix, "ClearChargingProfile");
    return file.is_null() ? nlohmann::json{{"status", "Accepted"}} : file;
}

nlohmann::json CPEmulator::on_data_transfer(Station& station, const nlohmann::json& payload)
{
    // vendorId is required per OCPP 1.6 §5.6
    if (!payload.contains("vendorId"))
        throw std::invalid_argument("missing vendorId");

    // Handle vendor-specific messageId dispatching
    if (payload.contains("messageId")) {
        auto message_id = payload.value("messageId", "");

        if (message_id == "SetStatusNotification" && payload.contains("data")) {
            try {
                auto data = nlohmann::json::parse(payload["data"].get<std::string>());
                if (data.contains("connectorId") && data.contains("status")) {
                    int connector_id = data["connectorId"].get<int>();
                    auto new_status = data["status"].get<std::string>();
                    auto now = std::chrono::system_clock::now();

                    // Update internal state for matching connectors
                    for (auto& conn : station.connector_states) {
                        if (connector_id == 0 || conn.connector_id == connector_id) {
                            conn.status = new_status;
                            conn.status_updated = now;
                        }
                    }

                    send_status_notification(station, connector_id, new_status);
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

nlohmann::json CPEmulator::on_get_composite_schedule(Station& station, const nlohmann::json& /*payload*/)
{
    auto file = load_response_file(station.prefix, "GetCompositeSchedule");
    return file.is_null() ? nlohmann::json{{"status", "Accepted"}} : file;
}

nlohmann::json CPEmulator::on_get_configuration(Station& station, const nlohmann::json& payload)
{
    nlohmann::json config_keys = nlohmann::json::array();
    nlohmann::json unknown_keys = nlohmann::json::array();

    if (payload.contains("key") && !payload["key"].is_null()) {
        // Accept both array ["k1","k2"] and single string "k1"
        nlohmann::json keys_to_find;
        if (payload["key"].is_array())
            keys_to_find = payload["key"];
        else if (payload["key"].is_string())
            keys_to_find = nlohmann::json::array({payload["key"]});

        if (!keys_to_find.empty()) {
            for (const auto& requested : keys_to_find) {
                auto k = requested.get<std::string>();
                auto it = station.config_keys.find(k);
                if (it != station.config_keys.end())
                    config_keys.push_back({{"key", k}, {"value", it->second.value}, {"readonly", it->second.readonly}});
                else
                    unknown_keys.push_back(k);
            }
        }
    } else {
        for (const auto& [k, cv] : station.config_keys)
            config_keys.push_back({{"key", k}, {"value", cv.value}, {"readonly", cv.readonly}});
    }

    nlohmann::json result = {{"configurationKey", config_keys}};
    if (!unknown_keys.empty())
        result["unknownKey"] = unknown_keys;
    return result;
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
        int cid = conn->connector_id;

        send_request(station, "Authorize", {{"idTag", id_tag}},
            [this, identity = station.identity, cid, id_tag](const WsMessage& resp) {
                auto* st = find_station(identity);
                if (!st) return;

                if (resp.type == WsMessage::Type::Error) {
                    app_->logger().warn("[{}] Authorize error: {} - {}",
                        identity, resp.error_code, resp.error_description);
                    return;
                }

                auto info = parse_id_tag_info(resp.payload);
                update_auth_cache(*st, id_tag, info);

                auto* conn = find_connector(*st, cid);
                if (!conn) return;

                if (info.status == "Accepted") {
                    continue_remote_start(*st, *conn);
                } else {
                    auto now = std::chrono::system_clock::now();
                    app_->logger().warn("[{}] Authorize rejected for {} on connector {}",
                        identity, id_tag, conn->connector_id);
                    conn->status = "Faulted";
                    conn->status_updated = now;
                    send_status_notification(*st, conn->connector_id, "Faulted");
                }
            });
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

    ConnectorState* conn = nullptr;
    if (connector_id == 0) {
        auto it = station.config_keys.find("ReserveConnectorZeroSupported");
        if (it == station.config_keys.end() || it->second.value != "true")
            return {{"status", "Rejected"}};
        // Use first connector as station-level reservation target
        if (!station.connector_states.empty())
            conn = &station.connector_states.front();
    } else {
        conn = find_connector(station, connector_id);
    }
    if (!conn)
        return {{"status", "Rejected"}};

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

    auto type = payload["type"].get<std::string>();
    app_->logger().info("[{}] Reset requested: {}", station.identity, type);

    // Stop active transactions on all connectors before reset
    for (auto& conn : station.connector_states) {
        if (conn.transaction_id >= 0)
            local_stop_transaction(station, conn, "Reboot");
    }

    // Schedule reconnect after response is sent (simulates reboot)
    // One-shot timer lets the CallResult be sent first
    loop_->add_timer(std::chrono::milliseconds(500), [this, identity = station.identity]() {
        auto* st = find_station(identity);
        if (!st) return;

        app_->logger().info("[{}] rebooting (closing connection)", st->identity);

        // Reset all connector states
        for (auto& conn : st->connector_states)
            conn.reset();

        st->boot_status = BootStatus::not_sent;
        st->next_boot_retry = {};
        st->next_meter_values = {};

        // Force reconnect — triggers full boot sequence (BootNotification + StatusNotification)
        if (st->ws)
            st->ws->reconnect();
    }, false);

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
        if (payload.contains("connectorId")) {
            // Specific connector (or connector 0 = station itself)
            send_status_notification(station, connector_id);
        } else {
            // No connectorId → send for station (0) + each connector (per OCPP 1.6 §5.17)
            send_status_notification(station, 0);
            for (auto& conn : station.connector_states)
                send_status_notification(station, conn.connector_id, conn.status);
        }
    } else if (requested == "DiagnosticsStatusNotification") {
        send_request(station, "DiagnosticsStatusNotification", {{"status", "Idle"}});
    } else if (requested == "FirmwareStatusNotification") {
        send_request(station, "FirmwareStatusNotification", {{"status", "Idle"}});
    } else if (requested == "MeterValues") {
        if (payload.contains("connectorId") && connector_id > 0) {
            // Specific connector
            auto* target = find_connector(station, connector_id);
            if (!target)
                return {{"status", "Rejected"}};
            send_meter_values(station, *target);
        } else {
            // No connectorId → send for all connectors with active charging (per OCPP 1.6 §5.17)
            bool sent = false;
            for (auto& cs : station.connector_states) {
                if (cs.status == "Charging" || cs.status == "SuspendedEVSE" || cs.status == "SuspendedEV") {
                    send_meter_values(station, cs);
                    sent = true;
                }
            }
            if (!sent)
                return {{"status", "Rejected"}};
        }
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

// ── OCPP 2.0.1 CSMS→CP handlers ────────────────────────────────────────────

nlohmann::json CPEmulator::on_request_start_transaction(Station& station, const nlohmann::json& payload)
{
    int evse_id = payload.value("evseId", 0);
    auto id_token = payload.value("idToken", nlohmann::json::object());
    auto id_tag = id_token.value("idToken", "");
    auto id_type = id_token.value("type", "ISO14443");
    int remote_start_id = payload.value("remoteStartId", 0);

    EvseState* evse = nullptr;
    if (evse_id > 0) {
        evse = find_evse(station, evse_id);
    } else {
        // Find first available EVSE
        for (auto& e : station.evses) {
            if (e.status == "Available" && e.transaction_id.empty()) {
                evse = &e;
                break;
            }
        }
    }

    if (!evse)
        return {{"status", "Rejected"}};

    // H03: check reservation
    if (evse->reservation) {
        bool token_match = (evse->reservation->id_token == id_tag);
        bool group_match = (!evse->reservation->group_id_token.empty() &&
                            evse->reservation->group_id_token == id_tag);
        if (!token_match && !group_match)
            return {{"status", "Rejected"}};
        // Reservation consumed by starting transaction
        evse->reservation.reset();
    }

    if (!evse->transaction_id.empty())
        return {{"status", "Rejected"}};

    evse->id_token = id_tag;
    evse->id_token_type = id_type;
    evse->remote_start_id = remote_start_id;

    // Read connectorId hint from CSMS customData (dual-cable EVSE support)
    if (payload.contains("customData"))
        evse->active_connector_id = payload["customData"].value("connectorId", 0);

    // Authorize then start
    send_authorize_201(station, *evse);

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_request_stop_transaction(Station& station, const nlohmann::json& payload)
{
    auto tx_id = payload.value("transactionId", "");

    for (auto& evse : station.evses) {
        if (evse.transaction_id == tx_id) {
            send_transaction_event(station, evse, "Ended", "RemoteStop");

            evse.reset();
            if (!evse.connectors.empty())
                send_status_notification_201(station, evse.evse_id, evse.connectors[0].connector_id);

            return {{"status", "Accepted"}};
        }
    }

    return {{"status", "Rejected"}};
}

nlohmann::json CPEmulator::on_reset_201(Station& station, const nlohmann::json& payload)
{
    auto type = payload.value("type", "Immediate");
    int evse_id = payload.value("evseId", 0);

    app_->logger().info("[{}] Reset (2.0.1): type={} evseId={}", station.identity, type, evse_id);

    if (evse_id > 0) {
        // Partial reset: specific EVSE
        auto* evse = find_evse(station, evse_id);
        if (!evse)
            return {{"status", "Rejected"}};

        if (!evse->transaction_id.empty())
            send_transaction_event(station, *evse, "Ended", "ResetCommand");
        evse->reset();
    } else {
        // Full reset: all EVSEs
        for (auto& evse : station.evses) {
            if (!evse.transaction_id.empty())
                send_transaction_event(station, evse, "Ended", "ResetCommand");
            evse.reset();
        }
    }

    // Schedule reconnect (one-shot)
    loop_->add_timer(std::chrono::seconds(2), [this, identity = station.identity] {
        auto* st = find_station(identity);
        if (st && st->ws)
            st->ws->reconnect();
    }, false);

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_set_variables(Station& station, const nlohmann::json& payload)
{
    nlohmann::json results = nlohmann::json::array();

    if (payload.contains("setVariableData") && payload["setVariableData"].is_array()) {
        for (const auto& item : payload["setVariableData"]) {
            auto component = item.value("component", nlohmann::json::object());
            auto variable = item.value("variable", nlohmann::json::object());
            auto comp_name = component.value("name", "");
            auto var_name = variable.value("name", "");
            int evse_id = 0;
            if (component.contains("evse"))
                evse_id = component["evse"].value("id", 0);

            auto* var = find_device_model_var(station, comp_name, var_name, evse_id);
            if (!var) {
                results.push_back({{"attributeStatus", "UnknownVariable"},
                                   {"component", component}, {"variable", variable}});
            } else if (var->readonly) {
                results.push_back({{"attributeStatus", "Rejected"},
                                   {"component", component}, {"variable", variable}});
            } else {
                var->value = item.value("attributeValue", "");
                results.push_back({{"attributeStatus", "Accepted"},
                                   {"component", component}, {"variable", variable}});

                // Sync runtime state
                if (comp_name == "OCPPCommCtrl" && var_name == "HeartbeatInterval") {
                    try { station.heartbeat_interval = std::stoi(var->value); } catch (...) {}
                }
            }
        }
    }

    return {{"setVariableResult", results}};
}

nlohmann::json CPEmulator::on_get_variables(Station& station, const nlohmann::json& payload)
{
    nlohmann::json results = nlohmann::json::array();

    if (payload.contains("getVariableData") && payload["getVariableData"].is_array()) {
        for (const auto& item : payload["getVariableData"]) {
            auto component = item.value("component", nlohmann::json::object());
            auto variable = item.value("variable", nlohmann::json::object());
            auto comp_name = component.value("name", "");
            auto var_name = variable.value("name", "");
            int evse_id = 0;
            if (component.contains("evse"))
                evse_id = component["evse"].value("id", 0);

            auto* var = find_device_model_var(station, comp_name, var_name, evse_id);
            if (var) {
                results.push_back({{"attributeStatus", "Accepted"},
                                   {"component", component}, {"variable", variable},
                                   {"attributeValue", var->value}});
            } else {
                results.push_back({{"attributeStatus", "UnknownVariable"},
                                   {"component", component}, {"variable", variable}});
            }
        }
    }

    return {{"getVariableResult", results}};
}

nlohmann::json CPEmulator::on_change_availability_201(Station& station, const nlohmann::json& payload)
{
    auto op_status = payload.value("operationalStatus", "Operative");
    int evse_id = 0;

    if (payload.contains("evse") && payload["evse"].is_object()) {
        evse_id = payload["evse"].value("id", 0);
    }

    std::string new_status = (op_status == "Inoperative") ? "Unavailable" : "Available";
    bool any_scheduled = false;

    if (evse_id > 0) {
        auto* evse = find_evse(station, evse_id);
        if (!evse)
            return {{"status", "Rejected"}};
        if (!evse->transaction_id.empty()) {
            evse->pending_availability = new_status;
            any_scheduled = true;
        } else {
            evse->status = new_status;
            // H01.FR.16/17: cancel reservation on Inoperative
            if (new_status == "Unavailable" && evse->reservation) {
                int res_id = evse->reservation->id;
                evse->reservation.reset();
                send_reservation_status_update(station, res_id, "Removed");
            }
            auto* dm_var = find_device_model_var(station, "EVSE", "AvailabilityState", evse->evse_id);
            if (dm_var) dm_var->value = new_status;
            if (!evse->connectors.empty())
                send_status_notification_201(station, evse->evse_id, evse->connectors[0].connector_id);
        }
    } else {
        for (auto& evse : station.evses) {
            if (!evse.transaction_id.empty()) {
                evse.pending_availability = new_status;
                any_scheduled = true;
            } else {
                evse.status = new_status;
                // H01.FR.16/17: cancel reservation on Inoperative
                if (new_status == "Unavailable" && evse.reservation) {
                    int res_id = evse.reservation->id;
                    evse.reservation.reset();
                    send_reservation_status_update(station, res_id, "Removed");
                }
                auto* dm_var = find_device_model_var(station, "EVSE", "AvailabilityState", evse.evse_id);
                if (dm_var) dm_var->value = new_status;
                for (auto& conn : evse.connectors)
                    send_status_notification_201(station, evse.evse_id, conn.connector_id);
            }
        }
    }

    return {{"status", any_scheduled ? "Scheduled" : "Accepted"}};
}

nlohmann::json CPEmulator::on_data_transfer_201(Station& station, const nlohmann::json& payload)
{
    app_->logger().info("[{}] DataTransfer (2.0.1): vendorId={} messageId={}",
        station.identity,
        payload.value("vendorId", ""),
        payload.value("messageId", ""));

    return {{"status", "Accepted"}};
}

// ── OCPP 2.0.1 Phase 2 — Provisioning ──────────────────────────────────────

nlohmann::json CPEmulator::on_get_base_report(Station& station, const nlohmann::json& payload)
{
    int request_id = payload.value("requestId", 0);

    // Send NotifyReport asynchronously (after returning the response)
    auto vars = station.device_model;  // copy

    loop_->add_timer(std::chrono::milliseconds(100),
        [this, identity = station.identity, request_id, vars = std::move(vars)]() {
            auto* st = find_station(identity);
            if (!st) return;
            send_notify_report(*st, request_id, vars);
        }, false);  // false = one-shot

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_get_report(Station& station, const nlohmann::json& payload)
{
    int request_id = payload.value("requestId", 0);

    std::vector<ReportVariable> filtered;

    if (payload.contains("componentVariable") && payload["componentVariable"].is_array()) {
        for (auto& cv : payload["componentVariable"]) {
            auto comp_name = cv.value("component", nlohmann::json::object()).value("name", "");
            auto var_name = cv.value("variable", nlohmann::json::object()).value("name", "");

            for (auto& v : station.device_model) {
                if ((comp_name.empty() || v.component == comp_name) &&
                    (var_name.empty() || v.variable == var_name)) {
                    filtered.push_back(v);
                }
            }
        }
    } else {
        filtered = station.device_model;
    }

    if (filtered.empty())
        return {{"status", "EmptyResultSet"}};

    loop_->add_timer(std::chrono::milliseconds(100),
        [this, identity = station.identity, request_id, filtered = std::move(filtered)]() {
            auto* st = find_station(identity);
            if (!st) return;
            send_notify_report(*st, request_id, filtered);
        }, false);

    return {{"status", "Accepted"}};
}

void CPEmulator::send_notify_report(Station& station, int request_id,
                                     const std::vector<ReportVariable>& variables)
{
    nlohmann::json report_data = nlohmann::json::array();

    for (auto& v : variables) {
        nlohmann::json entry = {
            {"component", {{"name", v.component}}},
            {"variable", {{"name", v.variable}}},
            {"variableAttribute", nlohmann::json::array({
                {
                    {"value", v.value},
                    {"mutability", v.mutability},
                    {"persistent", true},
                    {"constant", v.readonly}
                }
            })}
        };

        entry["variableCharacteristics"] = {
            {"dataType", v.data_type},
            {"supportsMonitoring", false}
        };

        if (v.evse_id > 0)
            entry["component"]["evse"] = {{"id", v.evse_id}};

        report_data.push_back(std::move(entry));
    }

    send_request(station, "NotifyReport", {
        {"requestId", request_id},
        {"seqNo", 0},
        {"tbc", false},
        {"generatedAt", iso_time_now()},
        {"reportData", std::move(report_data)}
    });
}

// ── OCPP 2.0.1 Phase 2 — Availability ──────────────────────────────────────

nlohmann::json CPEmulator::on_unlock_connector_201(Station& station, const nlohmann::json& payload)
{
    int evse_id = payload.value("evseId", 0);
    int connector_id = payload.value("connectorId", 0);

    auto* evse = find_evse(station, evse_id);
    if (!evse)
        return {{"status", "UnknownConnector"}};

    bool connector_found = false;
    for (auto& c : evse->connectors) {
        if (c.connector_id == connector_id) {
            connector_found = true;
            break;
        }
    }
    if (!connector_found)
        return {{"status", "UnknownConnector"}};

    if (!evse->transaction_id.empty()) {
        send_transaction_event(station, *evse, "Ended", "UnlockCommand");
        evse->reset();
        for (auto& c : evse->connectors)
            send_status_notification_201(station, evse->evse_id, c.connector_id);
    }

    return {{"status", "Unlocked"}};
}

nlohmann::json CPEmulator::on_trigger_message_201(Station& station, const nlohmann::json& payload)
{
    auto requested = payload.value("requestedMessage", "");

    int evse_id = 0;
    if (payload.contains("evse"))
        evse_id = payload["evse"].value("id", 0);

    if (requested == "BootNotification") {
        send_boot_notification_201(station);
    } else if (requested == "Heartbeat") {
        send_heartbeat_201(station);
    } else if (requested == "StatusNotification") {
        if (evse_id > 0) {
            auto* evse = find_evse(station, evse_id);
            if (evse && !evse->connectors.empty())
                send_status_notification_201(station, evse_id, evse->connectors[0].connector_id);
            else
                return {{"status", "Rejected"}};
        } else {
            for (auto& evse : station.evses)
                for (auto& c : evse.connectors)
                    send_status_notification_201(station, evse.evse_id, c.connector_id);
        }
    } else if (requested == "TransactionEvent") {
        for (auto& evse : station.evses) {
            if (!evse.transaction_id.empty())
                send_transaction_event(station, evse, "Updated", "Trigger");
        }
    } else if (requested == "MeterValues") {
        for (auto& evse : station.evses) {
            if (!evse.transaction_id.empty()) {
                send_request(station, "MeterValues", {
                    {"evseId", evse.evse_id},
                    {"meterValue", nlohmann::json::array({
                        {
                            {"timestamp", iso_time_now()},
                            {"sampledValue", nlohmann::json::array({
                                {{"value", evse.meter_value},
                                 {"measurand", "Energy.Active.Import.Register"},
                                 {"unitOfMeasure", {{"unit", "Wh"}}}}
                            })}
                        }
                    })}
                });
            }
        }
    } else if (requested == "FirmwareStatusNotification") {
        send_request(station, "FirmwareStatusNotification", {{"status", "Idle"}});
    } else {
        return {{"status", "NotImplemented"}};
    }

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_clear_cache_201(Station& station, const nlohmann::json& /*payload*/)
{
    station.auth_cache.clear();
    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_get_transaction_status(Station& station, const nlohmann::json& payload)
{
    nlohmann::json result = {{"messagesInQueue", false}};

    if (payload.contains("transactionId")) {
        auto tx_id = payload["transactionId"].get<std::string>();
        for (auto& evse : station.evses) {
            if (evse.transaction_id == tx_id) {
                result["ongoingIndicator"] = true;
                break;
            }
        }
    }

    return result;
}

// ── OCPP 2.0.1 Phase 2 — Local Auth List ───────────────────────────────────

nlohmann::json CPEmulator::on_send_local_list_201(Station& station, const nlohmann::json& payload)
{
    int version = payload.value("versionNumber", 0);
    auto update_type = payload.value("updateType", "Full");

    if (update_type == "Full") {
        station.local_auth_list.clear();
    }

    if (payload.contains("localAuthorizationList") && payload["localAuthorizationList"].is_array()) {
        for (auto& entry : payload["localAuthorizationList"]) {
            if (update_type == "Differential") {
                auto id = entry.value("idToken", nlohmann::json::object()).value("idToken", "");
                std::erase_if(station.local_auth_list, [&id](const nlohmann::json& e) {
                    return e.value("idToken", nlohmann::json::object()).value("idToken", "") == id;
                });
            }
            station.local_auth_list.push_back(entry);
        }
    }

    station.local_list_version = version;

    app_->logger().info("[{}] LocalAuthList updated: version={}, entries={}, type={}",
        station.identity, version, station.local_auth_list.size(), update_type);

    return {{"status", "Accepted"}};
}

nlohmann::json CPEmulator::on_get_local_list_version_201(Station& station, const nlohmann::json& /*payload*/)
{
    return {{"versionNumber", station.local_list_version}};
}

} // namespace apostol

#endif // WITH_POSTGRESQL
