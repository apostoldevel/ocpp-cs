#include "ocpp/schema_registry.hpp"
#include <fstream>
#include <fmt/format.h>

namespace ocpp
{

std::string SchemaRegistry::make_key(const std::string& version,
                                     const std::string& action,
                                     const std::string& direction)
{
    return fmt::format("{}/{}{}", version, action, direction);
}

void SchemaRegistry::load(const std::string& version,
                          const std::filesystem::path& dir)
{
    if (!std::filesystem::exists(dir))
        throw std::runtime_error(fmt::format("Schema directory not found: {}", dir.string()));

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;

        auto filename = entry.path().stem().string();  // e.g. "BootNotificationRequest"

        std::ifstream f(entry.path());
        if (!f.is_open()) continue;

        try {
            auto schema = nlohmann::json::parse(f);
            nlohmann::json_schema::json_validator validator;
            validator.set_root_schema(schema);
            validators_.emplace(fmt::format("{}/{}", version, filename),
                                std::move(validator));

            // 1.6 compatibility: "BootNotification.json" -> also register as "BootNotificationRequest"
            if (!filename.ends_with("Response") && !filename.ends_with("Request")) {
                try {
                    nlohmann::json_schema::json_validator validator2;
                    validator2.set_root_schema(schema);
                    validators_.emplace(fmt::format("{}/{}Request", version, filename),
                                        std::move(validator2));
                } catch (const std::exception& e2) {
                    fmt::print(stderr, "Warning: failed to create 1.6 alias {}Request: {}\n",
                               filename, e2.what());
                }
            }
        } catch (const std::exception& e) {
            fmt::print(stderr, "Warning: failed to load schema {}: {}\n",
                       entry.path().string(), e.what());
        }
    }
}

std::optional<std::string> SchemaRegistry::validate(
    const std::string& version,
    const std::string& action,
    const std::string& direction,
    const nlohmann::json& payload) const
{
    auto key = make_key(version, action, direction);
    auto it = validators_.find(key);
    if (it == validators_.end())
        return std::nullopt;  // No schema = no validation (passthrough)

    try {
        it->second.validate(payload);
        return std::nullopt;  // Valid
    } catch (const std::exception& e) {
        return e.what();
    }
}

bool SchemaRegistry::has_schema(const std::string& version,
                                const std::string& action,
                                const std::string& direction) const
{
    return validators_.contains(make_key(version, action, direction));
}

} // namespace ocpp
