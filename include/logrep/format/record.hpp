// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace logrep::format {

/// Maximum payload per record.
static constexpr std::size_t kMaxPayload = 240;

/// Record type tag.
enum class RecordType : std::uint8_t {
    NavState    = 1,
    Attitude    = 2,
    Engine      = 3,
    Health      = 4,
    Event       = 5,
    Marker      = 6,
    Custom      = 255,
};

/// Fixed-size binary record — the unit of logging.
///
/// Layout: [header (16 bytes)] [payload (240 bytes)] = 256 bytes per record.
/// Designed for sequential append, memory-mapped replay, and bit-exact comparison.
struct alignas(8) Record {
    // Header (16 bytes)
    std::uint64_t  timestamp_ns;   ///< CLOCK_MONOTONIC nanoseconds.
    RecordType     type;           ///< Record type tag.
    std::uint8_t   channel;        ///< Source channel / partition ID.
    std::uint16_t  payload_length; ///< Actual payload length.
    std::uint32_t  sequence;       ///< Monotonic record counter.

    // Payload
    std::array<std::uint8_t, kMaxPayload> payload;

    /// Write typed data into the payload.
    template <typename T>
    void set_payload(T const& data) {
        static_assert(sizeof(T) <= kMaxPayload, "payload exceeds record capacity");
        payload_length = static_cast<std::uint16_t>(sizeof(T));
        std::memcpy(payload.data(), &data, sizeof(T));
    }

    /// Read typed data from the payload.
    template <typename T>
    [[nodiscard]] T get_payload() const {
        T result{};
        std::memcpy(&result, payload.data(), sizeof(T));
        return result;
    }

    /// Compare two records for bit-exact equality.
    [[nodiscard]] bool operator==(Record const& other) const {
        return timestamp_ns == other.timestamp_ns && type == other.type &&
               channel == other.channel && payload_length == other.payload_length &&
               sequence == other.sequence &&
               std::memcmp(payload.data(), other.payload.data(), payload_length) == 0;
    }
};

static_assert(sizeof(Record) == 256, "Record must be exactly 256 bytes");

/// Convert RecordType to string.
[[nodiscard]] constexpr auto type_name(RecordType t) -> std::string_view {
    switch (t) {
        case RecordType::NavState: return "nav";
        case RecordType::Attitude: return "att";
        case RecordType::Engine:   return "eng";
        case RecordType::Health:   return "health";
        case RecordType::Event:    return "event";
        case RecordType::Marker:   return "marker";
        case RecordType::Custom:   return "custom";
    }
    return "unknown";
}

}  // namespace logrep::format
