// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#pragma once

#include <logrep/format/record.hpp>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace logrep::recorder {

inline std::uint64_t now_ns() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

/// Fixed-capacity flight data recorder.
///
/// Append-only, bounded memory, no dynamic allocation.
/// Records can be written to file for offline replay.
///
/// @tparam MaxRecords  Maximum number of records in the buffer.
template <std::size_t MaxRecords>
class Recorder {
public:
    /// Append a typed record.
    template <typename T>
    bool record(format::RecordType type, std::uint8_t channel, T const& data) {
        if (count_ >= MaxRecords) return false;

        auto& rec = buffer_[count_];
        rec.timestamp_ns  = now_ns();
        rec.type          = type;
        rec.channel       = channel;
        rec.sequence      = static_cast<std::uint32_t>(count_);
        rec.set_payload(data);

        ++count_;
        return true;
    }

    /// Append a raw record.
    bool record_raw(format::Record const& rec) {
        if (count_ >= MaxRecords) return false;
        buffer_[count_] = rec;
        ++count_;
        return true;
    }

    /// Append an event marker (no payload data).
    bool mark(std::uint8_t channel, std::uint64_t timestamp_ns) {
        if (count_ >= MaxRecords) return false;
        auto& rec = buffer_[count_];
        rec.timestamp_ns  = timestamp_ns;
        rec.type          = format::RecordType::Marker;
        rec.channel       = channel;
        rec.sequence      = static_cast<std::uint32_t>(count_);
        rec.payload_length = 0;
        ++count_;
        return true;
    }

    /// Write all records to a binary file.
    bool write_to_file(char const* path) const {
        FILE* f = std::fopen(path, "wb");
        if (!f) return false;
        std::size_t written = std::fwrite(buffer_.data(), sizeof(format::Record), count_, f);
        std::fclose(f);
        return written == count_;
    }

    [[nodiscard]] std::size_t count() const { return count_; }
    [[nodiscard]] std::size_t capacity() const { return MaxRecords; }
    [[nodiscard]] bool full() const { return count_ >= MaxRecords; }

    [[nodiscard]] format::Record const& at(std::size_t idx) const { return buffer_[idx]; }
    [[nodiscard]] format::Record const* data() const { return buffer_.data(); }

    void clear() { count_ = 0; }

private:
    std::array<format::Record, MaxRecords> buffer_{};
    std::size_t count_{0};
};

}  // namespace logrep::recorder
