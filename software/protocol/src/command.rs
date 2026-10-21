use std::io::{self, Read, Write};

use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};
use unit_intervals::UnitInterval;

use crate::wire::{read_bool, write_bool, DecodeError, WireDecode, WireEncode};

pub type Interval = UnitInterval<f32>;

/// Sent from the desktop app to the MCU on every control tick.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct DesiredState {
    pub speed: Interval,
    pub dir: Direction,
    pub freewheel: bool,
    pub run: bool,
}

impl WireEncode for DesiredState {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        w.write_f32::<LittleEndian>(self.speed.get())?;
        self.dir.encode_into(w)?;
        write_bool(w, self.freewheel)?;
        write_bool(w, self.run)?;
        Ok(())
    }
}

impl WireDecode for DesiredState {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let speed_raw = r.read_f32::<LittleEndian>()?;
        let dir = Direction::decode_from(r)?;
        let freewheel = read_bool(r)?;
        let run = read_bool(r)?;
        Ok(DesiredState {
            speed: Interval::saturating(speed_raw),
            dir,
            freewheel,
            run,
        })
    }
}

/// Direction to spin the motor
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Direction {
    Clockwise,
    CounterClockwise,
}

impl WireEncode for Direction {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        w.write_u8(match self {
            Direction::Clockwise => 0,
            Direction::CounterClockwise => 1,
        })
    }
}

impl WireDecode for Direction {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let tag = r.read_u8()?;
        match tag {
            0 => Ok(Direction::Clockwise),
            1 => Ok(Direction::CounterClockwise),
            _ => Err(DecodeError::InvalidTag(tag)),
        }
    }
}

/// What the MCU should do on its own if no valid `DesiredState` arrives within the deadman timeout
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeadmanPolicy {
    Hold,
    Freewheel,
    Stop,
}

impl WireEncode for DeadmanPolicy {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        w.write_u8(match self {
            DeadmanPolicy::Hold => 0,
            DeadmanPolicy::Freewheel => 1,
            DeadmanPolicy::Stop => 2,
        })
    }
}

impl WireDecode for DeadmanPolicy {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let tag = r.read_u8()?;
        match tag {
            0 => Ok(DeadmanPolicy::Hold),
            1 => Ok(DeadmanPolicy::Freewheel),
            2 => Ok(DeadmanPolicy::Stop),
            _ => Err(DecodeError::InvalidTag(tag)),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ConnectConfig {
    pub deadman_policy: DeadmanPolicy,
    pub deadman_timeout_ms: u16, // TODO: clamp this for safety on the MCU
}

impl WireEncode for ConnectConfig {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        self.deadman_policy.encode_into(w)?;
        w.write_u16::<LittleEndian>(self.deadman_timeout_ms)?;
        Ok(())
    }
}

impl WireDecode for ConnectConfig {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let deadman_policy = DeadmanPolicy::decode_from(r)?;
        let deadman_timeout_ms = r.read_u16::<LittleEndian>()?;
        Ok(ConnectConfig {
            deadman_policy,
            deadman_timeout_ms,
        })
    }
}

/// Messages the computer can send to the microcontroller
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ToMcu {
    /// Configure the microcontroller on startup
    Connect(ConnectConfig),
    /// Intentionally closing the link to the microcontroller
    ///
    /// This should hold last outputs instead of acting on the [`DeadmanPolicy`].
    Disconnect,
    /// State to run the motor controller at
    Command(DesiredState),
}

impl WireEncode for ToMcu {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()> {
        match self {
            ToMcu::Connect(c) => {
                w.write_u8(0)?;
                c.encode_into(w)
            }
            ToMcu::Disconnect => w.write_u8(1),
            ToMcu::Command(d) => {
                w.write_u8(2)?;
                d.encode_into(w)
            }
        }
    }
}

impl WireDecode for ToMcu {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError> {
        let tag = r.read_u8()?;
        match tag {
            0 => Ok(ToMcu::Connect(ConnectConfig::decode_from(r)?)),
            1 => Ok(ToMcu::Disconnect),
            2 => Ok(ToMcu::Command(DesiredState::decode_from(r)?)),
            _ => Err(DecodeError::InvalidTag(tag)),
        }
    }
}
