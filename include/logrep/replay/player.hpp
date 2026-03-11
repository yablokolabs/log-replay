// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#pragma once

#include <logrep/format/record.hpp>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <vector>

namespace logrep::replay {

/// Flight data replay player.
///
/// Reads a binary recording and replays records sequentially.
/// Supports filtering by type/channel, seeking, and bit-exact comparison.
class Player {
public:
    using RecordCallback = std::function<void(format::Record const&)>;

    /// Load records from a binary file.
    bool load(char const* path) {
        FILE* f = std::fopen(path, "rb");
        if (!f) return false;

        std::fseek(f, 0, SEEK_END);
        long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);

        std::size_t n_records = static_cast<std::size_t>(size) / sizeof(format::Record);
        records_.resize(n_records);
        std::size_t read = std::fread(records_.data(), sizeof(format::Record), n_records, f);
        std::fclose(f);

        records_.resize(read);
        cursor_ = 0;
        return read == n_records;
    }

    /// Load from an in-memory buffer (for testing).
    void load_from_buffer(format::Record const* data, std::size_t count) {
        records_.assign(data, data + count);
        cursor_ = 0;
    }

    /// Replay all records, calling the callback for each.
    std::size_t replay_all(RecordCallback const& cb) {
        std::size_t count = 0;
        for (auto const& rec : records_) {
            cb(rec);
            ++count;
        }
        cursor_ = records_.size();
        return count;
    }

    /// Replay records of a specific type.
    std::size_t replay_filtered(format::RecordType type, RecordCallback const& cb) {
        std::size_t count = 0;
        for (auto const& rec : records_) {
            if (rec.type == type) {
                cb(rec);
                ++count;
            }
        }
        return count;
    }

    /// Step one record forward. Returns false at end.
    bool step(RecordCallback const& cb) {
        if (cursor_ >= records_.size()) return false;
        cb(records_[cursor_]);
        ++cursor_;
        return true;
    }

    /// Seek to a specific record index.
    void seek(std::size_t idx) { cursor_ = idx < records_.size() ? idx : records_.size(); }

    /// Compare two recordings for bit-exact equality.
    [[nodiscard]] static bool compare(Player const& a, Player const& b) {
        if (a.record_count() != b.record_count()) return false;
        for (std::size_t i = 0; i < a.record_count(); ++i) {
            if (!(a.records_[i] == b.records_[i])) return false;
        }
        return true;
    }

    /// Find first record where two recordings diverge. Returns -1 if identical.
    [[nodiscard]] static long first_difference(Player const& a, Player const& b) {
        std::size_t n = std::min(a.record_count(), b.record_count());
        for (std::size_t i = 0; i < n; ++i) {
            if (!(a.records_[i] == b.records_[i])) return static_cast<long>(i);
        }
        if (a.record_count() != b.record_count()) return static_cast<long>(n);
        return -1;
    }

    [[nodiscard]] std::size_t record_count() const { return records_.size(); }
    [[nodiscard]] std::size_t cursor() const { return cursor_; }
    [[nodiscard]] format::Record const& at(std::size_t idx) const { return records_[idx]; }

    /// Duration in nanoseconds (last timestamp - first timestamp).
    [[nodiscard]] std::uint64_t duration_ns() const {
        if (records_.size() < 2) return 0;
        return records_.back().timestamp_ns - records_.front().timestamp_ns;
    }

private:
    std::vector<format::Record> records_;
    std::size_t cursor_{0};
};

}  // namespace logrep::replay
