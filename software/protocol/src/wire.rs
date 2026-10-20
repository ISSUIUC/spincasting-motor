use crc::{Crc, CRC_32_ISCSI};
use serde::{Deserialize, Serialize};

const CRC: Crc<u32> = Crc::<u32>::new(&CRC_32_ISCSI);

/// Generous upper bound on a serialized + CRC message for protocol
// TODO: need to add tests and maybe static assertions for this
pub const MAX_PAYLOAD_SIZE: usize = 64;

// COBS adds at most 1 overhead byte per 254 payload bytes, plus trailing zero sentinel
pub const MAX_FRAME_SIZE: usize = MAX_PAYLOAD_SIZE + 4;

#[derive(Debug)]
pub enum WireError {
    Postcard(postcard::Error),
    Cobs(cobs::DecodeError),
    BufferTooSmall,
}

impl From<postcard::Error> for WireError {
    fn from(e: postcard::Error) -> Self {
        WireError::Postcard(e)
    }
}

impl From<cobs::DecodeError> for WireError {
    fn from(e: cobs::DecodeError) -> Self {
        WireError::Cobs(e)
    }
}

/// Serializes `value` with a CRC32 trailer, COBS-frames it, and appends trailing zero delimiter
pub fn encode<T: Serialize>(
    value: &T,
    payload_scratch: &mut [u8; MAX_PAYLOAD_SIZE],
    out: &mut [u8; MAX_FRAME_SIZE],
) -> Result<usize, WireError> {
    let crc_bytes = postcard::to_slice_crc32(value, payload_scratch, CRC.digest())?;
    let frame_len = cobs::encode(crc_bytes, out);
    if frame_len >= out.len() {
        return Err(WireError::BufferTooSmall);
    }
    out[frame_len] = 0;
    Ok(frame_len + 1)
}

/// Decodes one COBS-framed message, given the bytes received up to (but not incl.) the 0x00 delimiter
pub fn decode<'a, T: Deserialize<'a>>(
    frame: &[u8],
    payload_scratch: &'a mut [u8; MAX_PAYLOAD_SIZE],
) -> Result<T, WireError> {
    let report = cobs::decode(frame, payload_scratch)?;
    let crc_bytes = &payload_scratch[..report.frame_size()];
    Ok(postcard::from_bytes_crc32(crc_bytes, CRC.digest())?)
}
