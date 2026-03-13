#pragma once
//
// OCPP 1.5/1.6 Protocol Types and JSON Wire Format
//
// Ported from v1 ChargePoint.hpp — protocol enums, message structs,
// and JSON array <-> structured message conversion.
//

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <stdexcept>
#include <random>
#include <sstream>
#include <iomanip>

namespace ocpp
{

// ── Protocol & Message Types ────────────────────────────────────────────────

enum class ProtocolType { SOAP, JSON };

enum class MessageType { Call = 2, CallResult = 3, CallError = 4 };

// ── OCPP Error Codes (1.6 spec) ────────────────────────────────────────────

namespace error
{
    inline constexpr const char* NotImplemented            = "NotImplemented";
    inline constexpr const char* NotSupported              = "NotSupported";
    inline constexpr const char* InternalError             = "InternalError";
    inline constexpr const char* ProtocolError             = "ProtocolError";
    inline constexpr const char* SecurityError             = "SecurityError";
    inline constexpr const char* FormationViolation        = "FormationViolation";
    inline constexpr const char* PropertyConstraintViolation = "PropertyConstraintViolation";
    inline constexpr const char* OccurrenceConstraintViolation = "OccurrenceConstraintViolation";
    inline constexpr const char* TypeConstraintViolation   = "TypeConstraintViolation";
    inline constexpr const char* GenericError              = "GenericError";
} // namespace error

// ── OCPP Status Enums ──────────────────────────────────────────────────────

enum class ChargePointStatus {
    Available, Reserved, Preparing, Charging,
    SuspendedEVSE, SuspendedEV, Finishing,
    Unavailable, Faulted
};

inline std::string_view charge_point_status_to_string(ChargePointStatus s)
{
    switch (s) {
        case ChargePointStatus::Available:     return "Available";
        case ChargePointStatus::Reserved:      return "Reserved";
        case ChargePointStatus::Preparing:     return "Preparing";
        case ChargePointStatus::Charging:      return "Charging";
        case ChargePointStatus::SuspendedEVSE: return "SuspendedEVSE";
        case ChargePointStatus::SuspendedEV:   return "SuspendedEV";
        case ChargePointStatus::Finishing:     return "Finishing";
        case ChargePointStatus::Unavailable:   return "Unavailable";
        case ChargePointStatus::Faulted:       return "Faulted";
    }
    return "Unknown";
}

inline ChargePointStatus string_to_charge_point_status(std::string_view s)
{
    if (s == "Available")     return ChargePointStatus::Available;
    if (s == "Reserved")      return ChargePointStatus::Reserved;
    if (s == "Preparing")     return ChargePointStatus::Preparing;
    if (s == "Charging")      return ChargePointStatus::Charging;
    if (s == "SuspendedEVSE") return ChargePointStatus::SuspendedEVSE;
    if (s == "SuspendedEV")   return ChargePointStatus::SuspendedEV;
    if (s == "Finishing")     return ChargePointStatus::Finishing;
    if (s == "Unavailable")   return ChargePointStatus::Unavailable;
    if (s == "Faulted")       return ChargePointStatus::Faulted;
    return ChargePointStatus::Unavailable;
}

enum class ChargePointErrorCode {
    ConnectorLockFailure, HighTemperature, Mode3Error, NoError,
    PowerMeterFailure, PowerSwitchFailure, ReaderFailure, ResetFailure,
    GroundFailure, OverCurrentFailure, UnderVoltage, WeakSignal, OtherError
};

inline std::string_view charge_point_error_code_to_string(ChargePointErrorCode e)
{
    switch (e) {
        case ChargePointErrorCode::ConnectorLockFailure: return "ConnectorLockFailure";
        case ChargePointErrorCode::HighTemperature:      return "HighTemperature";
        case ChargePointErrorCode::Mode3Error:           return "Mode3Error";
        case ChargePointErrorCode::NoError:              return "NoError";
        case ChargePointErrorCode::PowerMeterFailure:    return "PowerMeterFailure";
        case ChargePointErrorCode::PowerSwitchFailure:   return "PowerSwitchFailure";
        case ChargePointErrorCode::ReaderFailure:        return "ReaderFailure";
        case ChargePointErrorCode::ResetFailure:         return "ResetFailure";
        case ChargePointErrorCode::GroundFailure:        return "GroundFailure";
        case ChargePointErrorCode::OverCurrentFailure:   return "OverCurrentFailure";
        case ChargePointErrorCode::UnderVoltage:         return "UnderVoltage";
        case ChargePointErrorCode::WeakSignal:           return "WeakSignal";
        case ChargePointErrorCode::OtherError:           return "OtherError";
    }
    return "OtherError";
}

inline ChargePointErrorCode string_to_charge_point_error_code(std::string_view s)
{
    if (s == "ConnectorLockFailure") return ChargePointErrorCode::ConnectorLockFailure;
    if (s == "HighTemperature")      return ChargePointErrorCode::HighTemperature;
    if (s == "Mode3Error")           return ChargePointErrorCode::Mode3Error;
    if (s == "NoError")              return ChargePointErrorCode::NoError;
    if (s == "PowerMeterFailure")    return ChargePointErrorCode::PowerMeterFailure;
    if (s == "PowerSwitchFailure")   return ChargePointErrorCode::PowerSwitchFailure;
    if (s == "ReaderFailure")        return ChargePointErrorCode::ReaderFailure;
    if (s == "ResetFailure")         return ChargePointErrorCode::ResetFailure;
    if (s == "GroundFailure")        return ChargePointErrorCode::GroundFailure;
    if (s == "OverCurrentFailure")   return ChargePointErrorCode::OverCurrentFailure;
    if (s == "UnderVoltage")         return ChargePointErrorCode::UnderVoltage;
    if (s == "WeakSignal")           return ChargePointErrorCode::WeakSignal;
    return ChargePointErrorCode::OtherError;
}

enum class RegistrationStatus { Accepted, Pending, Rejected };

inline std::string_view registration_status_to_string(RegistrationStatus s)
{
    switch (s) {
        case RegistrationStatus::Accepted: return "Accepted";
        case RegistrationStatus::Pending:  return "Pending";
        case RegistrationStatus::Rejected: return "Rejected";
    }
    return "Rejected";
}

inline RegistrationStatus string_to_registration_status(std::string_view s)
{
    if (s == "Accepted") return RegistrationStatus::Accepted;
    if (s == "Pending")  return RegistrationStatus::Pending;
    return RegistrationStatus::Rejected;
}

enum class AuthorizationStatus { Accepted, Blocked, Expired, Invalid, ConcurrentTx };

inline std::string_view authorization_status_to_string(AuthorizationStatus s)
{
    switch (s) {
        case AuthorizationStatus::Accepted:     return "Accepted";
        case AuthorizationStatus::Blocked:      return "Blocked";
        case AuthorizationStatus::Expired:      return "Expired";
        case AuthorizationStatus::Invalid:      return "Invalid";
        case AuthorizationStatus::ConcurrentTx: return "ConcurrentTx";
    }
    return "Invalid";
}

inline AuthorizationStatus string_to_authorization_status(std::string_view s)
{
    if (s == "Accepted")     return AuthorizationStatus::Accepted;
    if (s == "Blocked")      return AuthorizationStatus::Blocked;
    if (s == "Expired")      return AuthorizationStatus::Expired;
    if (s == "ConcurrentTx") return AuthorizationStatus::ConcurrentTx;
    return AuthorizationStatus::Invalid;
}

enum class ResetType { Hard, Soft };

inline std::string_view reset_type_to_string(ResetType t)
{
    return t == ResetType::Hard ? "Hard" : "Soft";
}

inline ResetType string_to_reset_type(std::string_view s)
{
    return s == "Soft" ? ResetType::Soft : ResetType::Hard;
}

enum class RemoteStartStopStatus { Accepted, Rejected };
enum class ReservationStatus { Accepted, Faulted, Occupied, Rejected, Unavailable };
enum class CancelReservationStatus { Accepted, Rejected };
enum class ClearCacheStatus { Accepted, Rejected };
enum class ConfigurationStatus { Accepted, Rejected, RebootRequired, NotSupported };
enum class TriggerMessageStatus { Accepted, Rejected, NotImplemented };

enum class MessageTrigger {
    BootNotification, DiagnosticsStatusNotification,
    FirmwareStatusNotification, Heartbeat,
    MeterValues, StatusNotification
};

// ── SOAP Message (OCPP 1.5) ────────────────────────────────────────────────

#ifdef WITH_POSTGRESQL
struct SOAPMessage {
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::pair<std::string, std::string>> values;
    std::string notification;

    std::string get_value(std::string_view key) const
    {
        for (const auto& [k, v] : values)
            if (k == key) return v;
        return {};
    }

    void set_value(const std::string& key, const std::string& val)
    {
        for (auto& [k, v] : values) {
            if (k == key) { v = val; return; }
        }
        values.emplace_back(key, val);
    }

    std::string get_header(std::string_view key) const
    {
        for (const auto& [k, v] : headers)
            if (k == key) return v;
        return {};
    }

    void set_header(const std::string& key, const std::string& val)
    {
        for (auto& [k, v] : headers) {
            if (k == key) { v = val; return; }
        }
        headers.emplace_back(key, val);
    }
};
#endif

// ── OCPP JSON Message (wire format) ────────────────────────────────────────

struct OcppMessage {
    MessageType    type = MessageType::Call;
    std::string    unique_id;
    std::string    action;           // only for Call
    std::string    error_code;       // only for CallError
    std::string    error_description;// only for CallError
    nlohmann::json payload = nlohmann::json::object();
};

// ── JSON Wire Format: Parse / Serialize ─────────────────────────────────────

inline OcppMessage parse_ocpp_json(std::string_view text)
{
    auto arr = nlohmann::json::parse(text);

    if (!arr.is_array() || arr.size() < 3)
        throw std::invalid_argument("OCPP message must be a JSON array with at least 3 elements");

    OcppMessage msg;
    int type_id = arr[0].get<int>();

    switch (type_id) {
    case 2: // Call
        if (arr.size() < 4)
            throw std::invalid_argument("OCPP Call must have 4 elements");
        msg.type      = MessageType::Call;
        msg.unique_id = arr[1].get<std::string>();
        msg.action    = arr[2].get<std::string>();
        msg.payload   = arr[3];
        break;

    case 3: // CallResult
        msg.type      = MessageType::CallResult;
        msg.unique_id = arr[1].get<std::string>();
        msg.payload   = arr[2];
        break;

    case 4: // CallError
        if (arr.size() < 5)
            throw std::invalid_argument("OCPP CallError must have 5 elements");
        msg.type              = MessageType::CallError;
        msg.unique_id         = arr[1].get<std::string>();
        msg.error_code        = arr[2].get<std::string>();
        msg.error_description = arr[3].get<std::string>();
        msg.payload           = arr[4];
        break;

    default:
        throw std::invalid_argument("Unknown OCPP message type: " + std::to_string(type_id));
    }

    return msg;
}

inline std::string serialize_ocpp_json(const OcppMessage& msg)
{
    nlohmann::json arr;

    switch (msg.type) {
    case MessageType::Call:
        arr = nlohmann::json::array({
            static_cast<int>(msg.type),
            msg.unique_id,
            msg.action,
            msg.payload
        });
        break;

    case MessageType::CallResult:
        arr = nlohmann::json::array({
            static_cast<int>(msg.type),
            msg.unique_id,
            msg.payload
        });
        break;

    case MessageType::CallError:
        arr = nlohmann::json::array({
            static_cast<int>(msg.type),
            msg.unique_id,
            msg.error_code,
            msg.error_description,
            msg.payload
        });
        break;
    }

    return arr.dump();
}

// ── Unique ID Generator ─────────────────────────────────────────────────────

inline std::string generate_unique_id()
{
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << dist(gen);
    return oss.str();
}

// ── Convenience Constructors ────────────────────────────────────────────────

inline OcppMessage make_call(std::string_view action, nlohmann::json payload)
{
    OcppMessage msg;
    msg.type      = MessageType::Call;
    msg.unique_id = generate_unique_id();
    msg.action    = std::string(action);
    msg.payload   = std::move(payload);
    return msg;
}

inline OcppMessage make_call_result(std::string_view unique_id, nlohmann::json payload)
{
    OcppMessage msg;
    msg.type      = MessageType::CallResult;
    msg.unique_id = std::string(unique_id);
    msg.payload   = std::move(payload);
    return msg;
}

inline OcppMessage make_call_error(
    std::string_view unique_id,
    std::string_view error_code,
    std::string_view error_description,
    nlohmann::json details = nlohmann::json::object())
{
    OcppMessage msg;
    msg.type              = MessageType::CallError;
    msg.unique_id         = std::string(unique_id);
    msg.error_code        = std::string(error_code);
    msg.error_description = std::string(error_description);
    msg.payload           = std::move(details);
    return msg;
}

} // namespace ocpp
