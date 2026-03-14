#pragma once

#include <chrono>
#include <ctime>
#include <string>

#include <fmt/format.h>

namespace ocpp
{

/// Format current UTC time as ISO 8601 string with milliseconds.
/// Optional @p delta_seconds shifts the timestamp (e.g. +300 for expiry).
inline std::string iso_time_now(int delta_seconds = 0)
{
    auto now = std::chrono::system_clock::now();
    if (delta_seconds != 0)
        now += std::chrono::seconds(delta_seconds);

    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm utc{};
    gmtime_r(&tt, &utc);

    return fmt::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
        utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
        utc.tm_hour, utc.tm_min, utc.tm_sec, static_cast<int>(ms.count()));
}

/// Parse ISO 8601 UTC string (YYYY-MM-DDTHH:MM:SS) to time_point.
/// Milliseconds/timezone suffix are ignored.
inline std::chrono::system_clock::time_point parse_iso_time(const std::string& s)
{
    std::tm tm{};
    if (strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &tm))
        return std::chrono::system_clock::from_time_t(timegm(&tm));
    return {};
}

} // namespace ocpp
