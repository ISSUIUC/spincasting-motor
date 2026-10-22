#include "motor_control.h"

#include <Arduino.h>

#include "pwm.h"

namespace motor_control {

namespace {

const int PIN_DIR = 7;       // MCU_DIR       -> LSS -> TB3 pin 2 (DIRECTION)
const int PIN_FREEWHEEL = 6; // MCU_FREEWHEEL -> LSS -> TB3 pin 3 (FREEWHEEL)
const int PIN_RUNSTOP = 8;   // MCU_RUNSTOP   -> LSS -> TB3 pin 4 (RUN/STOP)
const int PIN_PGIN = 4;      // MCU_PG_IN     <- TB3 pin 1 (PG OUT)
const int PIN_FAULTIN = 2;   // MCU_FAULT_IN  <- TB3 pin 5 (FAULT OUT)
const int PIN_VCONTROL = 9;  // MCU_V_CONTROL -> TB3 pin 6 (V CONTROL), direct wire, no LSS

const float VCONTROL_FREQ_HZ = 2000.0;

PwmOut pwmVControl(PIN_VCONTROL);

// debounced rising-edge counter for a digital input pin
class PulseCounter {
public:
    explicit PulseCounter(int pin) : pin_(pin) {}

    void begin() { last_state_ = digitalRead(pin_); }

    void poll() {
        bool state = digitalRead(pin_);
        if (state == HIGH && last_state_ == LOW) {
            count_++;
        }
        last_state_ = state;
    }

    uint32_t count() const { return count_; }

private:
    int pin_;
    bool last_state_ = LOW;
    uint32_t count_ = 0;
};

PulseCounter pgCounter(PIN_PGIN);

} // namespace

void setup() {
    pinMode(PIN_DIR, OUTPUT);
    pinMode(PIN_FREEWHEEL, OUTPUT);
    pinMode(PIN_RUNSTOP, OUTPUT);
    pinMode(PIN_PGIN, INPUT);
    pinMode(PIN_FAULTIN, INPUT);

    digitalWrite(PIN_DIR, LOW);
    digitalWrite(PIN_FREEWHEEL, LOW);
    digitalWrite(PIN_RUNSTOP, LOW);

    pwmVControl.begin(VCONTROL_FREQ_HZ, 0.0);
    pgCounter.begin();
}

void apply(const protocol::DesiredState& state) {
    // TODO: confirm which value is "clockwise"
    digitalWrite(PIN_DIR, state.dir == protocol::Direction::Clockwise ? LOW : HIGH);
    digitalWrite(PIN_FREEWHEEL, state.freewheel ? HIGH : LOW);
    digitalWrite(PIN_RUNSTOP, state.run ? HIGH : LOW);

    float dutyPct = state.speed * 100.0f;
    if (dutyPct < 0.0f) dutyPct = 0.0f;
    if (dutyPct > 100.0f) dutyPct = 100.0f;
    pwmVControl.pulse_perc(dutyPct);
}

void poll() {
    pgCounter.poll();
}

bool fault() {
    return digitalRead(PIN_FAULTIN) == HIGH;
}

uint32_t motor_pulse_count() {
    return pgCounter.count();
}

} // namespace motor_control
