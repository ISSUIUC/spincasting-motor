#include <optional>
#include <type_traits>
#include <variant>

#include "motor_control.h"
#include "protocol_messages.h"
#include "protocol_wire.h"

namespace {

template <typename>
inline constexpr bool always_false_v = false;

const uint32_t FIRMWARE_BUILD_ID = 1;
const unsigned long TELEMETRY_INTERVAL_MS = 50;

uint8_t rxBuf[protocol::MAX_FRAME_SIZE];
size_t rxLen = 0;

uint8_t payloadScratch[protocol::MAX_PAYLOAD_SIZE];
uint8_t txFrame[protocol::MAX_FRAME_SIZE];

unsigned long lastTelemetryMs = 0;

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
                sendFromMcu(protocol::FromMcu::make_hello({FIRMWARE_BUILD_ID}));
            } else if constexpr (std::is_same_v<T, protocol::ToMcu::Disconnect>) {
                // hold last outputs
            } else if constexpr (std::is_same_v<T, protocol::DesiredState>) {
                motor_control::apply(payload);
            } else {
                static_assert(always_false_v<T>, "unhandled ToMcu payload type");
            }
        },
        msg.payload);
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
        // Ooerflowed without seeing a delimiter, drop and resync on the next 0x00
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
