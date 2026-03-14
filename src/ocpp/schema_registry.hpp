#pragma once

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <string>
#include <optional>
#include <unordered_map>
#include <filesystem>

namespace ocpp
{

// SchemaRegistry -- loads OCPP JSON schemas from disk and validates messages.
//
// Usage:
//   SchemaRegistry registry;
//   registry.load("1.6", "/etc/cs/schemas/1.6");
//   registry.load("2.0.1", "/etc/cs/schemas/2.0.1");
//   auto err = registry.validate("1.6", "BootNotification", "Request", payload);
//   if (err) { /* send CallError(FormatViolation, *err) */ }
//
class SchemaRegistry
{
public:
    // Load all *.json schemas from directory for given OCPP version.
    // Schema files named "{Action}Request.json" / "{Action}Response.json"
    // (2.0.1) or "{Action}.json" (1.6 request) / "{Action}Response.json" (1.6 response).
    void load(const std::string& version, const std::filesystem::path& dir);

    // Validate payload against schema for (version, action, direction).
    // Returns std::nullopt if valid, error string if invalid.
    // direction: "Request" for CP->CSMS, "Response" for CSMS->CP replies.
    std::optional<std::string> validate(const std::string& version,
                                        const std::string& action,
                                        const std::string& direction,
                                        const nlohmann::json& payload) const;

    // Check if schema exists for (version, action, direction).
    bool has_schema(const std::string& version,
                    const std::string& action,
                    const std::string& direction) const;

    // Number of loaded schemas.
    std::size_t schema_count() const { return validators_.size(); }

private:
    // Key: "version/ActionDirection" e.g. "2.0.1/BootNotificationRequest"
    static std::string make_key(const std::string& version,
                                const std::string& action,
                                const std::string& direction);

    std::unordered_map<std::string,
        nlohmann::json_schema::json_validator> validators_;
};

} // namespace ocpp
