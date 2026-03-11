// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#include <logrep/recorder/recorder.hpp>
#include <logrep/replay/player.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__);                \
            std::abort();                                                                 \
        }                                                                                 \
    } while (0)

struct TestNav {
    double lat, lon, alt;
};

// --- Test 1: Record and read back ---
static void test_record_readback() {
    logrep::recorder::Recorder<64> rec;

    TestNav nav{48.8566, 2.3522, 10000.0};
    CHECK(rec.record(logrep::format::RecordType::NavState, 0, nav));
    CHECK(rec.count() == 1);

    auto readback = rec.at(0).get_payload<TestNav>();
    CHECK(std::abs(readback.lat - 48.8566) < 1e-9);
    CHECK(std::abs(readback.lon - 2.3522) < 1e-9);
    CHECK(std::abs(readback.alt - 10000.0) < 1e-9);
    CHECK(rec.at(0).type == logrep::format::RecordType::NavState);

    std::printf("  PASS: record and readback\n");
}

// --- Test 2: Capacity enforcement ---
static void test_capacity() {
    logrep::recorder::Recorder<4> rec;

    TestNav nav{0, 0, 0};
    CHECK(rec.record(logrep::format::RecordType::NavState, 0, nav));
    CHECK(rec.record(logrep::format::RecordType::NavState, 0, nav));
    CHECK(rec.record(logrep::format::RecordType::NavState, 0, nav));
    CHECK(rec.record(logrep::format::RecordType::NavState, 0, nav));
    CHECK(!rec.record(logrep::format::RecordType::NavState, 0, nav));  // Full
    CHECK(rec.full());
    CHECK(rec.count() == 4);

    std::printf("  PASS: capacity enforcement\n");
}

// --- Test 3: File write + replay ---
static void test_file_roundtrip() {
    // Record
    logrep::recorder::Recorder<16> rec;
    for (int i = 0; i < 10; ++i) {
        TestNav nav{static_cast<double>(i), static_cast<double>(i) * 0.1, static_cast<double>(i) * 100.0};
        rec.record(logrep::format::RecordType::NavState, 0, nav);
    }

    CHECK(rec.write_to_file("/tmp/test_logrep.bin"));

    // Replay
    logrep::replay::Player player;
    CHECK(player.load("/tmp/test_logrep.bin"));
    CHECK(player.record_count() == 10);

    std::size_t count = 0;
    player.replay_all([&](logrep::format::Record const& r) {
        auto nav = r.get_payload<TestNav>();
        CHECK(std::abs(nav.lat - static_cast<double>(count)) < 1e-9);
        ++count;
    });
    CHECK(count == 10);

    std::printf("  PASS: file write + replay roundtrip\n");
}

// --- Test 4: Filtered replay ---
static void test_filtered_replay() {
    logrep::recorder::Recorder<16> rec;

    TestNav nav{0, 0, 0};
    double eng_val = 2400.0;

    rec.record(logrep::format::RecordType::NavState, 0, nav);
    rec.record(logrep::format::RecordType::Engine, 1, eng_val);
    rec.record(logrep::format::RecordType::NavState, 0, nav);
    rec.record(logrep::format::RecordType::Engine, 1, eng_val);
    rec.record(logrep::format::RecordType::NavState, 0, nav);

    logrep::replay::Player player;
    player.load_from_buffer(rec.data(), rec.count());

    std::size_t nav_count = player.replay_filtered(
        logrep::format::RecordType::NavState,
        [](logrep::format::Record const&) {});

    CHECK(nav_count == 3);

    std::size_t eng_count = player.replay_filtered(
        logrep::format::RecordType::Engine,
        [](logrep::format::Record const&) {});

    CHECK(eng_count == 2);

    std::printf("  PASS: filtered replay (3 nav, 2 engine)\n");
}

// --- Test 5: Bit-exact comparison ---
static void test_bit_exact_comparison() {
    logrep::recorder::Recorder<8> rec1;
    logrep::recorder::Recorder<8> rec2;

    // Same data, same order
    for (int i = 0; i < 5; ++i) {
        logrep::format::Record r{};
        r.timestamp_ns  = static_cast<std::uint64_t>(i) * 1000;
        r.type          = logrep::format::RecordType::NavState;
        r.channel       = 0;
        r.sequence      = static_cast<std::uint32_t>(i);
        r.payload_length = 8;
        double val = static_cast<double>(i);
        std::memcpy(r.payload.data(), &val, sizeof(val));

        rec1.record_raw(r);
        rec2.record_raw(r);
    }

    logrep::replay::Player p1, p2;
    p1.load_from_buffer(rec1.data(), rec1.count());
    p2.load_from_buffer(rec2.data(), rec2.count());

    CHECK(logrep::replay::Player::compare(p1, p2));
    CHECK(logrep::replay::Player::first_difference(p1, p2) == -1);

    std::printf("  PASS: bit-exact comparison (identical)\n");
}

// --- Test 6: Divergence detection ---
static void test_divergence_detection() {
    logrep::recorder::Recorder<8> rec1;
    logrep::recorder::Recorder<8> rec2;

    for (int i = 0; i < 5; ++i) {
        logrep::format::Record r{};
        r.timestamp_ns  = static_cast<std::uint64_t>(i) * 1000;
        r.type          = logrep::format::RecordType::NavState;
        r.channel       = 0;
        r.sequence      = static_cast<std::uint32_t>(i);
        r.payload_length = 8;
        double val = static_cast<double>(i);
        std::memcpy(r.payload.data(), &val, sizeof(val));
        rec1.record_raw(r);

        // Corrupt record 3 in the second recording
        if (i == 3) val += 0.001;
        std::memcpy(r.payload.data(), &val, sizeof(val));
        rec2.record_raw(r);
    }

    logrep::replay::Player p1, p2;
    p1.load_from_buffer(rec1.data(), rec1.count());
    p2.load_from_buffer(rec2.data(), rec2.count());

    CHECK(!logrep::replay::Player::compare(p1, p2));
    CHECK(logrep::replay::Player::first_difference(p1, p2) == 3);

    std::printf("  PASS: divergence detection (at record 3)\n");
}

int main() {
    std::printf("log-replay tests\n");
    std::printf("=================\n");

    test_record_readback();
    test_capacity();
    test_file_roundtrip();
    test_filtered_replay();
    test_bit_exact_comparison();
    test_divergence_detection();

    std::printf("\nAll 6 tests passed.\n");
    return 0;
}
