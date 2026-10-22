#include "wire.h"

#include <cstring>
#include <optional>

namespace protocol {

bool ByteWriter::write_u8(uint8_t v) {
    if (pos_ + 1 > buf_.size()) return false;
    buf_[pos_++] = v;
    return true;
}

bool ByteWriter::write_u16_le(uint16_t v) {
    if (pos_ + 2 > buf_.size()) return false;
    buf_[pos_++] = static_cast<uint8_t>(v);
    buf_[pos_++] = static_cast<uint8_t>(v >> 8);
    return true;
}

bool ByteWriter::write_u32_le(uint32_t v) {
    if (pos_ + 4 > buf_.size()) return false;
    buf_[pos_++] = static_cast<uint8_t>(v);
    buf_[pos_++] = static_cast<uint8_t>(v >> 8);
    buf_[pos_++] = static_cast<uint8_t>(v >> 16);
    buf_[pos_++] = static_cast<uint8_t>(v >> 24);
    return true;
}

bool ByteWriter::write_f32_le(float v) {
    static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return write_u32_le(bits);
}

bool ByteWriter::write_bool(bool v) {
    return write_u8(v ? 1 : 0);
}

std::optional<uint8_t> ByteReader::read_u8() {
    if (pos_ + 1 > buf_.size()) return std::nullopt;
    return buf_[pos_++];
}

std::optional<uint16_t> ByteReader::read_u16_le() {
    if (pos_ + 2 > buf_.size()) return std::nullopt;
    uint16_t v = static_cast<uint16_t>(buf_[pos_] | (static_cast<unsigned>(buf_[pos_ + 1]) << 8));
    pos_ += 2;
    return v;
}

std::optional<uint32_t> ByteReader::read_u32_le() {
    if (pos_ + 4 > buf_.size()) return std::nullopt;
    uint32_t v = static_cast<uint32_t>(buf_[pos_]) | (static_cast<uint32_t>(buf_[pos_ + 1]) << 8) |
                 (static_cast<uint32_t>(buf_[pos_ + 2]) << 16) | (static_cast<uint32_t>(buf_[pos_ + 3]) << 24);
    pos_ += 4;
    return v;
}

std::optional<float> ByteReader::read_f32_le() {
    static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
    std::optional<uint32_t> bits = read_u32_le();
    if (!bits) return std::nullopt;
    float v;
    std::memcpy(&v, &*bits, sizeof(v));
    return v;
}

std::optional<bool> ByteReader::read_bool() {
    std::optional<uint8_t> v = read_u8();
    if (!v) return std::nullopt;
    if (*v != 0 && *v != 1) return std::nullopt;
    return *v == 1;
}

uint32_t crc32c(Span<const uint8_t> data) {
    static uint32_t table[256];
    static bool table_init = false;
    if (!table_init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? (0x82F63B78u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        table_init = true;
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < data.size(); i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

std::optional<size_t> cobs_encode(Span<const uint8_t> input, Span<uint8_t> output) {
    if (output.empty()) return std::nullopt;

    size_t read_index = 0;
    size_t write_index = 1; // output[0] reserved for the first code byte
    size_t code_index = 0;
    uint8_t code = 1;

    while (read_index < input.size()) {
        if (input[read_index] == 0) {
            output[code_index] = code;
            code = 1;
            if (write_index >= output.size()) return std::nullopt;
            code_index = write_index++;
            read_index++;
        } else {
            if (write_index >= output.size()) return std::nullopt;
            output[write_index++] = input[read_index++];
            code++;
            if (code == 0xFF) {
                output[code_index] = code;
                code = 1;
                if (write_index >= output.size()) return std::nullopt;
                code_index = write_index++;
            }
        }
    }
    output[code_index] = code;
    return write_index;
}

std::optional<size_t> cobs_decode(Span<const uint8_t> input, Span<uint8_t> output) {
    size_t read_index = 0;
    size_t write_index = 0;

    while (read_index < input.size()) {
        uint8_t code = input[read_index];
        if (code == 0) return std::nullopt; // a literal zero byte can't appear inside a frame
        if (read_index + code > input.size() && code != 1) return std::nullopt;
        read_index++;

        for (uint8_t i = 1; i < code; i++) {
            if (write_index >= output.size()) return std::nullopt;
            output[write_index++] = input[read_index++];
        }
        if (code != 0xFF && read_index != input.size()) {
            if (write_index >= output.size()) return std::nullopt;
            output[write_index++] = 0;
        }
    }
    return write_index;
}

} // namespace protocol
