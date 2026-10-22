#pragma once
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

#include "protocol_wire.h"

namespace protocol {

enum class Direction : uint8_t {
    Clockwise = 0,
    CounterClockwise = 1,
};

// enum classes don't allow methods
bool encode_direction(ByteWriter& w, Direction d);
std::optional<Direction> decode_direction(ByteReader& r);

enum class DeadmanPolicy : uint8_t {
    Hold = 0,
    Freewheel = 1,
    Stop = 2,
};

// enum classes don't allow methods
bool encode_deadman_policy(ByteWriter& w, DeadmanPolicy p);
std::optional<DeadmanPolicy> decode_deadman_policy(ByteReader& r);

// Sent from the desktop app to the MCU on every control tick
struct DesiredState {
    float speed = 0.0f; // raw [0.0, 1.0] fraction
    Direction dir = Direction::Clockwise;
    bool freewheel = false;
    bool run = false;

    bool encode_into(ByteWriter& w) const;
    static std::optional<DesiredState> decode_from(ByteReader& r);
};

// What the MCU should do on its own if no valid DesiredState arrives within the deadman timeout.
struct ConnectConfig {
    DeadmanPolicy deadman_policy = DeadmanPolicy::Hold;
    uint16_t deadman_timeout_ms = 0;

    bool encode_into(ByteWriter& w) const;
    static std::optional<ConnectConfig> decode_from(ByteReader& r);
};

// Messages the computer can send to the MCU
struct ToMcu {
    // Disconnect carries no data; aliased (not a distinct struct) for the
    // same reason Interval aliases UnitInterval in command.rs - it's just a
    // clearer name over a general-purpose type, swappable for a real struct
    // later by touching only this line if Disconnect ever needs a payload.
    using Disconnect = std::monostate;
    using Payload = std::variant<ConnectConfig, Disconnect, DesiredState>;
    // the index also matches the on wire tag of the variant
    static_assert(std::is_same_v<std::variant_alternative_t<0, Payload>, ConnectConfig>);
    static_assert(std::is_same_v<std::variant_alternative_t<1, Payload>, Disconnect>);
    static_assert(std::is_same_v<std::variant_alternative_t<2, Payload>, DesiredState>);

    Payload payload;

    bool encode_into(ByteWriter& w) const;
    static std::optional<ToMcu> decode_from(ByteReader& r);

    static ToMcu make_connect(ConnectConfig c) { return ToMcu{c}; }
    static ToMcu make_disconnect() { return ToMcu{Disconnect{}}; }
    static ToMcu make_command(DesiredState d) { return ToMcu{d}; }
};

// Streamed continuously from the MCU to the desktop app
struct Telemetry {
    uint32_t mcu_tick_us = 0;
    uint32_t motor_pulse_count = 0;
    uint32_t tach_pulse_count = 0;
    bool fault = false;

    bool encode_into(ByteWriter& w) const;
    static std::optional<Telemetry> decode_from(ByteReader& r);
};

// Sent in response to ConnectConfig, verifies MCU version
struct HelloInfo {
    uint32_t firmware_build_id = 0;

    bool encode_into(ByteWriter& w) const;
    static std::optional<HelloInfo> decode_from(ByteReader& r);
};

// Messages the MCU can send to the computer
struct FromMcu {
    using Payload = std::variant<HelloInfo, Telemetry>;
    // the index also matches the on wire tag of the variant
    static_assert(std::is_same_v<std::variant_alternative_t<0, Payload>, HelloInfo>);
    static_assert(std::is_same_v<std::variant_alternative_t<1, Payload>, Telemetry>);

    Payload payload;

    bool encode_into(ByteWriter& w) const;
    static std::optional<FromMcu> decode_from(ByteReader& r);

    static FromMcu make_hello(HelloInfo h) { return FromMcu{h}; }
    static FromMcu make_telemetry(Telemetry t) { return FromMcu{t}; }
};

} // namespace protocol
