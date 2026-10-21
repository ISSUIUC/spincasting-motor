use std::io::{self, Read, Write};

use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};

use crate::wire::{read_bool, write_bool, DecodeError, WireDecode, WireEncode};

/// Streamed continuously from the MCU to the desktop app.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
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

impl WireEncode for Telemetry {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        w.write_u32::<LittleEndian>(self.mcu_tick_us)?;
        w.write_u32::<LittleEndian>(self.motor_pulse_count)?;
        w.write_u32::<LittleEndian>(self.tach_pulse_count)?;
        write_bool(w, self.fault)?;
        Ok(())
    }
}

impl WireDecode for Telemetry {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let mcu_tick_us = r.read_u32::<LittleEndian>()?;
        let motor_pulse_count = r.read_u32::<LittleEndian>()?;
        let tach_pulse_count = r.read_u32::<LittleEndian>()?;
        let fault = read_bool(r)?;
        Ok(Telemetry {
            mcu_tick_us,
            motor_pulse_count,
            tach_pulse_count,
            fault,
        })
    }
}

/// Sent in response to [`ConnectConfig`](super::command::ConnectConfig)
///
/// Verifies MCU version
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct HelloInfo {
    pub firmware_build_id: u32,
}

impl WireEncode for HelloInfo {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        w.write_u32::<LittleEndian>(self.firmware_build_id)
    }
}

impl WireDecode for HelloInfo {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let firmware_build_id = r.read_u32::<LittleEndian>()?;
        Ok(HelloInfo { firmware_build_id })
    }
}

/// Messages the microcontroller can send to the computer
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FromMcu {
    Hello(HelloInfo),
    Telemetry(Telemetry),
}

impl WireEncode for FromMcu {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        match self {
            FromMcu::Hello(h) => {
                w.write_u8(0)?;
                h.encode_into(w)
            }
            FromMcu::Telemetry(t) => {
                w.write_u8(1)?;
                t.encode_into(w)
            }
        }
    }
}

impl WireDecode for FromMcu {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let tag = r.read_u8()?;
        match tag {
            0 => Ok(FromMcu::Hello(HelloInfo::decode_from(r)?)),
            1 => Ok(FromMcu::Telemetry(Telemetry::decode_from(r)?)),
            _ => Err(DecodeError::InvalidTag(tag)),
        }
    }
}
