use std::io::{self, Cursor, Read, Write};

use byteorder::WriteBytesExt;
use crc::{Crc, CRC_32_ISCSI};

const CRC: Crc<u32> = Crc::<u32>::new(&CRC_32_ISCSI);

/// Generous upper bound on a serialized + CRC message for protocol
// TODO: need to add tests and maybe static assertions for this
pub const MAX_PAYLOAD_SIZE: usize = 64;

// COBS adds at most 1 overhead byte per 254 payload bytes, plus trailing zero sentinel
pub const MAX_FRAME_SIZE: usize = MAX_PAYLOAD_SIZE + 4;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DecodeError {
    UnexpectedEnd,
    InvalidTag(u8),
    InvalidBool(u8),
}

impl From<io::Error> for DecodeError {
    fn from(e: io::Error) -> Self {
        debug_assert_eq!(
            e.kind(),
            io::ErrorKind::UnexpectedEof,
            "decode_from only ever reads from an in-memory Cursor<&[u8]>, which can't fail any other way"
        );
        DecodeError::UnexpectedEnd
    }
}

/// Fixed-width, little-endian serialization
pub trait WireEncode {
    fn encode_into<W: Write>(&self, w: &mut W) -> io::Result<()>;
}

pub trait WireDecode: Sized {
    fn decode_from<R: Read>(r: &mut R) -> Result<Self, DecodeError>;
}

/// `byteorder` has no `bool` primitive
pub(crate) fn write_bool<W: Write>(w: &mut W, v: bool) -> io::Result<()> {
    w.write_u8(v as u8)
}

pub(crate) fn read_bool<R: Read>(r: &mut R) -> Result<bool, DecodeError> {
    let mut buf = [0u8; 1];
    r.read_exact(&mut buf)?;
    match buf[0] {
        0 => Ok(false),
        1 => Ok(true),
        other => Err(DecodeError::InvalidBool(other)),
    }
}

#[derive(Debug)]
pub enum WireError {
    Cobs(cobs::DecodeError),
    Decode(DecodeError),
    Io(io::Error),
    BufferTooSmall,
    TooShort,
    CrcMismatch,
    TrailingBytes,
}

impl From<cobs::DecodeError> for WireError {
    fn from(e: cobs::DecodeError) -> Self {
        WireError::Cobs(e)
    }
}

impl From<DecodeError> for WireError {
    fn from(e: DecodeError) -> Self {
        WireError::Decode(e)
    }
}

impl From<io::Error> for WireError {
    fn from(e: io::Error) -> Self {
        WireError::Io(e)
    }
}

/// Serializes `value` with a CRC32 trailer, COBS-frames it, and appends trailing zero delimiter
pub fn encode<T: WireEncode>(
    value: &T,
    payload_scratch: &mut [u8; MAX_PAYLOAD_SIZE],
    out: &mut [u8; MAX_FRAME_SIZE],
) -> Result<usize, WireError> {
    let len = {
        let mut cursor = Cursor::new(&mut payload_scratch[..]);
        value.encode_into(&mut cursor)?;
        cursor.position() as usize
    };

    let crc = CRC.checksum(&payload_scratch[..len]);
    payload_scratch[len..len + 4].copy_from_slice(&crc.to_le_bytes());
    let total = len + 4;

    let frame_len = cobs::encode(&payload_scratch[..total], out);
    if frame_len >= out.len() {
        return Err(WireError::BufferTooSmall);
    }
    out[frame_len] = 0;
    Ok(frame_len + 1)
}

/// Decodes one COBS-framed message, given the bytes received up to (but not incl.) the 0x00 delimiter
pub fn decode<T: WireDecode>(
    frame: &[u8],
    payload_scratch: &mut [u8; MAX_PAYLOAD_SIZE],
) -> Result<T, WireError> {
    let report = cobs::decode(frame, payload_scratch)?;
    let total = report.frame_size();
    if total < 4 {
        return Err(WireError::TooShort);
    }
    let (data, crc_bytes) = payload_scratch[..total].split_at(total - 4);
    let expected_crc = u32::from_le_bytes(crc_bytes.try_into().unwrap());
    let actual_crc = CRC.checksum(data);
    if actual_crc != expected_crc {
        return Err(WireError::CrcMismatch);
    }

    let mut cursor = Cursor::new(data);
    let value = T::decode_from(&mut cursor)?;
    if cursor.position() != data.len() as u64 {
        return Err(WireError::TrailingBytes);
    }
    Ok(value)
}
