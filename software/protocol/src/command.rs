use serde::{Deserialize, Serialize};
use unit_intervals::UnitInterval;

pub type Interval = UnitInterval<f32>;

/// Sent from the desktop app to the MCU on every control tick.
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct DesiredState {
    pub speed: Interval,
    pub dir: Direction,
    pub freewheel: bool,
    pub run: bool,
}

/// Direction to spin the motor
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Direction {
    Clockwise,
    CounterClockwise,
}

/// What the MCU should do on its own if no valid `DesiredState` arrives within the deadman timeout
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DeadmanPolicy {
    Hold,
    Freewheel,
    Stop,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct ConnectConfig {
    pub deadman_policy: DeadmanPolicy,
    pub deadman_timeout_ms: u16, // TODO: clamp this for safety on the MCU
}

/// Messages the computer can send to the microcontroller
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub enum ToMcu {
    /// Configure the microcontroller on startup
    Connect(ConnectConfig),
    /// State to run the motor controller at
    Command(DesiredState),
    /// Intentionally closing the link to the microcontroller
    /// 
    /// This should hold last outputs instead of acting on the [`DeadmanPolicy`].
    Disconnect,
}
