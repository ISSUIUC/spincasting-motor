use serde::{Deserialize, Serialize};

/// Streamed continuously from the MCU to the desktop app.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct Telemetry {
    /// MCU local tick counter
    /// 
    /// Needs to be unwrapped on the computer, because will overflow every ~70m.
    pub mcu_tick_us: u32,

    /// Cumulative motor pulse count
    pub motor_pulse_count: u32,
    /// Cumulative tachometer pulse count
    pub tach_pulse_count: u32,
    
    pub fault: bool,
}

/// Sent in response to [`ConnectConfig`](super::command::ConnectConfig)
/// 
/// Verifies MCU version
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct HelloInfo {
    pub firmware_build_id: u32,
}

/// Messages the microcontroller can send to the computer
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum FromMcu {
    Hello(HelloInfo),
    Telemetry(Telemetry),
}
