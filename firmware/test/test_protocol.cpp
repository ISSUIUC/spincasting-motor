// Host-side test, built with CMake + Catch2 (no Arduino toolchain involved)
// - decodes each shared fixture in protocol-fixtures/, checks the decoded
// fields, then re-encodes and checks the bytes match the fixture exactly.
// This is the C++ half of the cross-language verification; software/protocol
// has the matching Rust-side round trip over the same files.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <variant>
#include <vector>

#include "../src/protocol/messages.h"
#include "../src/protocol/wire.h"

using namespace protocol;

namespace {

const std::filesystem::path FIXTURES_DIR = PROTOCOL_FIXTURES_DIR;

std::vector<uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Decodes `name`, checks it re-encodes back to the exact same bytes, and
// returns the decoded value for the caller to check its fields.
template <typename T>
T decodeFixture(const std::string& name) {
    // Fixtures include the trailing 0x00 delimiter; decode() takes the frame
    // up to but not including it, matching wire.rs's documented contract.
    std::vector<uint8_t> fileBytes = readFile(FIXTURES_DIR / name);
    REQUIRE(!fileBytes.empty());
    REQUIRE(fileBytes.back() == 0);

    uint8_t payloadScratch[MAX_PAYLOAD_SIZE];
    Expected<T, DecodeError> decoded = decode<T>(Span(fileBytes.data(), fileBytes.size() - 1), payloadScratch);
    REQUIRE(decoded.has_value());

    uint8_t txFrame[MAX_FRAME_SIZE];
    std::optional<size_t> encodedLen = encode(*decoded, payloadScratch, txFrame);
    REQUIRE(encodedLen.has_value());
    CHECK(*encodedLen == fileBytes.size());
    CHECK(memcmp(txFrame, fileBytes.data(), *encodedLen) == 0);

    return *decoded;
}

} // namespace

TEST_CASE("connect_hold_250.bin round-trips", "[protocol][to_mcu]") {
    ToMcu m = decodeFixture<ToMcu>("connect_hold_250.bin");
    auto* c = std::get_if<ConnectConfig>(&m.payload);
    REQUIRE(c);
    CHECK(c->deadman_policy == DeadmanPolicy::Hold);
    CHECK(c->deadman_timeout_ms == 250);
}

TEST_CASE("disconnect.bin round-trips", "[protocol][to_mcu]") {
    ToMcu m = decodeFixture<ToMcu>("disconnect.bin");
    CHECK(std::holds_alternative<ToMcu::Disconnect>(m.payload));
}

TEST_CASE("command_zero_ccw_freewheel.bin round-trips", "[protocol][to_mcu]") {
    ToMcu m = decodeFixture<ToMcu>("command_zero_ccw_freewheel.bin");
    auto* d = std::get_if<DesiredState>(&m.payload);
    REQUIRE(d);
    CHECK(d->speed == 0.0f);
    CHECK(d->dir == Direction::CounterClockwise);
    CHECK(d->freewheel == true);
    CHECK(d->run == false);
}

TEST_CASE("command_full_cw_run.bin round-trips", "[protocol][to_mcu]") {
    ToMcu m = decodeFixture<ToMcu>("command_full_cw_run.bin");
    auto* d = std::get_if<DesiredState>(&m.payload);
    REQUIRE(d);
    CHECK(d->speed == 1.0f);
    CHECK(d->dir == Direction::Clockwise);
    CHECK(d->freewheel == false);
    CHECK(d->run == true);
}

TEST_CASE("hello_build_12345678.bin round-trips", "[protocol][from_mcu]") {
    FromMcu m = decodeFixture<FromMcu>("hello_build_12345678.bin");
    auto* h = std::get_if<HelloInfo>(&m.payload);
    REQUIRE(h);
    CHECK(h->firmware_build_id == 0x12345678u);
}

TEST_CASE("telemetry_near_wrap_fault.bin round-trips", "[protocol][from_mcu]") {
    FromMcu m = decodeFixture<FromMcu>("telemetry_near_wrap_fault.bin");
    auto* t = std::get_if<Telemetry>(&m.payload);
    REQUIRE(t);
    CHECK(t->mcu_tick_us == 4000000000u);
    CHECK(t->motor_pulse_count == 1u);
    CHECK(t->tach_pulse_count == 0u);
    CHECK(t->fault == true);
}
