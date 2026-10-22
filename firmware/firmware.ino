#include <optional>
#include <type_traits>
#include <variant>

#include "motor_control.h"
#include "src/protocol/messages.h"
#include "src/protocol/wire.h"

namespace {

template <typename>
inline constexpr bool always_false_v = false;

const uint32_t FIRMWARE_BUILD_ID = 1;
const unsigned long TELEMETRY_INTERVAL_MS = 50;

// minimum time of waiting for message from computer before enacting deadman policy
const uint16_t MIN_DEADMAN_TIMEOUT_MS = 50;

uint8_t rxBuf[protocol::MAX_FRAME_SIZE];
size_t rxLen = 0;

uint8_t payloadScratch[protocol::MAX_PAYLOAD_SIZE];
uint8_t txFrame[protocol::MAX_FRAME_SIZE];

unsigned long lastTelemetryMs = 0;

struct McuState {
    bool connected = false;
    protocol::DeadmanPolicy deadman_policy = protocol::DeadmanPolicy::Hold;
    uint16_t deadman_timeout_ms = 0;
    protocol::DesiredState last_command{};
    unsigned long last_command_at_ms = 0;
};

McuState state;

void sendFromMcu(const protocol::FromMcu& msg) {
    std::optional<size_t> len = protocol::encode(msg, payloadScratch, txFrame);
    if (len) {
        Serial.write(txFrame, *len);
    }
}

void handleToMcu(const protocol::ToMcu& msg) {
    std::visit(
        [](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, protocol::ConnectConfig>) {
                state.connected = true;
                state.deadman_policy = payload.deadman_policy;
                state.deadman_timeout_ms = payload.deadman_timeout_ms < MIN_DEADMAN_TIMEOUT_MS
                                                ? MIN_DEADMAN_TIMEOUT_MS
                                                : payload.deadman_timeout_ms;
                state.last_command_at_ms = millis(); // don't immediately time out right after connecting
                sendFromMcu(protocol::FromMcu::make_hello({FIRMWARE_BUILD_ID}));
            } else if constexpr (std::is_same_v<T, protocol::ToMcu::Disconnect>) {
                // hold last outputs and stop enforcing the deadman timeout, this is an intentional disconnect, not lost connectivity.
                state.connected = false;
            } else if constexpr (std::is_same_v<T, protocol::DesiredState>) {
                state.last_command = payload;
                state.last_command_at_ms = millis();
                motor_control::apply(payload);
            } else {
                static_assert(always_false_v<T>, "unhandled ToMcu payload type");
            }
        },
        msg.payload);
}

// enforces deadman_policy once deadman_timeout_ms has elapsed since the last valid DesiredState;
void checkDeadman() {
    if (!state.connected) return;
    if (millis() - state.last_command_at_ms < state.deadman_timeout_ms) return;

    switch (state.deadman_policy) {
        case protocol::DeadmanPolicy::Hold:
            break; // hardware already holds its last-applied outputs
        case protocol::DeadmanPolicy::Freewheel: {
            protocol::DesiredState d = state.last_command;
            d.freewheel = true;
            d.run = false;
            motor_control::apply(d);
            break;
        }
        case protocol::DeadmanPolicy::Stop: {
            protocol::DesiredState d = state.last_command;
            d.run = false;
            motor_control::apply(d);
            break;
        }
    }
}

void handleIncomingByte(uint8_t b) {
    if (b == 0) {
        if (rxLen > 0) {
            auto msg = protocol::decode<protocol::ToMcu>(protocol::Span(rxBuf, rxLen), payloadScratch);
            if (msg) {
                handleToMcu(*msg);
            }
        }
        rxLen = 0;
        return;
    }
    if (rxLen < sizeof(rxBuf)) {
        rxBuf[rxLen++] = b;
    } else {
        // overflowed without seeing a delimiter, drop and resync on the next 0x00
        rxLen = 0;
    }
}

} // namespace

void setup() {
    Serial.begin(115200);
    motor_control::setup();
}

void loop() {
    motor_control::poll();

    while (Serial.available()) {
        handleIncomingByte(static_cast<uint8_t>(Serial.read()));
    }

    checkDeadman();

    unsigned long now = millis();
    if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryMs = now;
        protocol::Telemetry t;
        t.mcu_tick_us = micros();
        t.motor_pulse_count = motor_control::motor_pulse_count();
        t.tach_pulse_count = motor_control::tach_pulse_count();
        t.fault = motor_control::fault();
        sendFromMcu(protocol::FromMcu::make_telemetry(t));
    }
}
