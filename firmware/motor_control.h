#pragma once
#include <cstdint>

#include "src/protocol/messages.h"

namespace motor_control {

void setup();

void apply(const protocol::DesiredState& state);

// Call every loop() iteration
// 
// polls PG_IN for a rising edge, until the wire is moved to a pin with hardware interrupts
void poll();

bool fault();
uint32_t motor_pulse_count();

// TODO: connect tachometer, once wired up, needs a PulseCounter on its pin
inline uint32_t tach_pulse_count() { return 0; }

} // namespace motor_control
