#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

#include "../core/span.h"
#include "../core/expected.h"

// protocol from MCU <-> computer communication, must be in sync with the definintion on the Rust side
namespace protocol {

// mirrors constants in wire.rs.
constexpr size_t MAX_PAYLOAD_SIZE = 64;
constexpr size_t MAX_FRAME_SIZE = MAX_PAYLOAD_SIZE + 4;

enum class DecodeError {
    Cobs,          // malformed COBS framing
    TooShort,      // decoded payload shorter than the CRC trailer alone
    CrcMismatch,
    BadMessage,    // the message itself didn't parse
    TrailingBytes, // message used fewer bytes than were sent
};

class ByteWriter {
public:
    explicit ByteWriter(Span<uint8_t> buf) : buf_(buf), pos_(0) {}

    bool write_u8(uint8_t v);
    bool write_u16_le(uint16_t v);
    bool write_u32_le(uint32_t v);
    bool write_f32_le(float v);
    bool write_bool(bool v);

    size_t size() const { return pos_; }

private:
    Span<uint8_t> buf_;
    size_t pos_;
};

class ByteReader {
public:
    explicit ByteReader(Span<const uint8_t> buf) : buf_(buf), pos_(0) {}

    std::optional<uint8_t> read_u8();
    std::optional<uint16_t> read_u16_le();
    std::optional<uint32_t> read_u32_le();
    std::optional<float> read_f32_le();
    std::optional<bool> read_bool();

    size_t position() const { return pos_; }

private:
    Span<const uint8_t> buf_;
    size_t pos_;
};

// CRC-32C (Castagnoli) - CRC_32_ISCSI
uint32_t crc32c(Span<const uint8_t> data);

// nullopt on failure (malformed input or `output` too small)
std::optional<size_t> cobs_encode(Span<const uint8_t> input, Span<uint8_t> output);
std::optional<size_t> cobs_decode(Span<const uint8_t> input, Span<uint8_t> output);

template <typename T>
std::optional<size_t> encode(const T& value, Span<uint8_t> payload_scratch, Span<uint8_t> out) {
    ByteWriter w(payload_scratch);
    if (!value.encode_into(w)) return std::nullopt;
    size_t len = w.size();
    if (len + 4 > payload_scratch.size()) return std::nullopt;

    uint32_t crc = crc32c(payload_scratch.subspan(0, len));
    payload_scratch[len + 0] = static_cast<uint8_t>(crc);
    payload_scratch[len + 1] = static_cast<uint8_t>(crc >> 8);
    payload_scratch[len + 2] = static_cast<uint8_t>(crc >> 16);
    payload_scratch[len + 3] = static_cast<uint8_t>(crc >> 24);
    size_t total = len + 4;

    std::optional<size_t> frame_len = cobs_encode(payload_scratch.subspan(0, total), out);
    if (!frame_len || *frame_len >= out.size()) return std::nullopt;
    out[*frame_len] = 0;
    return *frame_len + 1;
}

template <typename T>
Expected<T, DecodeError> decode(Span<const uint8_t> frame, Span<uint8_t> payload_scratch) {
    std::optional<size_t> total = cobs_decode(frame, payload_scratch);
    if (!total) return DecodeError::Cobs;
    if (*total < 4) return DecodeError::TooShort;

    size_t data_len = *total - 4;
    uint32_t expected_crc = static_cast<uint32_t>(payload_scratch[data_len + 0]) |
                             (static_cast<uint32_t>(payload_scratch[data_len + 1]) << 8) |
                             (static_cast<uint32_t>(payload_scratch[data_len + 2]) << 16) |
                             (static_cast<uint32_t>(payload_scratch[data_len + 3]) << 24);
    uint32_t actual_crc = crc32c(payload_scratch.subspan(0, data_len));
    if (actual_crc != expected_crc) return DecodeError::CrcMismatch;

    ByteReader r(payload_scratch.subspan(0, data_len));
    std::optional<T> value = T::decode_from(r);
    if (!value) return DecodeError::BadMessage;
    if (r.position() != data_len) return DecodeError::TrailingBytes;
    return *value;
}

} // namespace protocol
