#include "messages.h"

namespace protocol {

bool encode_direction(ByteWriter& w, Direction d) {
    return w.write_u8(static_cast<uint8_t>(d));
}

std::optional<Direction> decode_direction(ByteReader& r) {
    std::optional<uint8_t> tag = r.read_u8();
    if (!tag || *tag > 1) return std::nullopt;
    return static_cast<Direction>(*tag);
}

bool encode_deadman_policy(ByteWriter& w, DeadmanPolicy p) {
    return w.write_u8(static_cast<uint8_t>(p));
}

std::optional<DeadmanPolicy> decode_deadman_policy(ByteReader& r) {
    std::optional<uint8_t> tag = r.read_u8();
    if (!tag || *tag > 2) return std::nullopt;
    return static_cast<DeadmanPolicy>(*tag);
}

bool DesiredState::encode_into(ByteWriter& w) const {
    if (!w.write_f32_le(speed)) return false;
    if (!encode_direction(w, dir)) return false;
    if (!w.write_bool(freewheel)) return false;
    if (!w.write_bool(run)) return false;
    return true;
}

std::optional<DesiredState> DesiredState::decode_from(ByteReader& r) {
    std::optional<float> speed = r.read_f32_le();
    if (!speed) return std::nullopt;
    std::optional<Direction> dir = decode_direction(r);
    if (!dir) return std::nullopt;
    std::optional<bool> freewheel = r.read_bool();
    if (!freewheel) return std::nullopt;
    std::optional<bool> run = r.read_bool();
    if (!run) return std::nullopt;

    DesiredState out;
    out.speed = *speed;
    out.dir = *dir;
    out.freewheel = *freewheel;
    out.run = *run;
    return out;
}

bool ConnectConfig::encode_into(ByteWriter& w) const {
    if (!encode_deadman_policy(w, deadman_policy)) return false;
    if (!w.write_u16_le(deadman_timeout_ms)) return false;
    return true;
}

std::optional<ConnectConfig> ConnectConfig::decode_from(ByteReader& r) {
    std::optional<DeadmanPolicy> policy = decode_deadman_policy(r);
    if (!policy) return std::nullopt;
    std::optional<uint16_t> timeout = r.read_u16_le();
    if (!timeout) return std::nullopt;

    ConnectConfig out;
    out.deadman_policy = *policy;
    out.deadman_timeout_ms = *timeout;
    return out;
}

bool ToMcu::encode_into(ByteWriter& w) const {
    if (!w.write_u8(static_cast<uint8_t>(payload.index()))) return false;
    return std::visit(
        [&w](const auto& alt) {
            using T = std::decay_t<decltype(alt)>;
            if constexpr (std::is_same_v<T, ToMcu::Disconnect>) {
                return true; // Disconnect carries no payload
            } else {
                return alt.encode_into(w);
            }
        },
        payload);
}

std::optional<ToMcu> ToMcu::decode_from(ByteReader& r) {
    std::optional<uint8_t> tag = r.read_u8();
    if (!tag) return std::nullopt;
    switch (*tag) {
        case 0: {
            std::optional<ConnectConfig> c = ConnectConfig::decode_from(r);
            if (!c) return std::nullopt;
            return make_connect(*c);
        }
        case 1:
            return make_disconnect();
        case 2: {
            std::optional<DesiredState> d = DesiredState::decode_from(r);
            if (!d) return std::nullopt;
            return make_command(*d);
        }
        default:
            return std::nullopt;
    }
}

bool Telemetry::encode_into(ByteWriter& w) const {
    if (!w.write_u32_le(mcu_tick_us)) return false;
    if (!w.write_u32_le(motor_pulse_count)) return false;
    if (!w.write_u32_le(tach_pulse_count)) return false;
    if (!w.write_bool(fault)) return false;
    return true;
}

std::optional<Telemetry> Telemetry::decode_from(ByteReader& r) {
    std::optional<uint32_t> mcu_tick_us = r.read_u32_le();
    if (!mcu_tick_us) return std::nullopt;
    std::optional<uint32_t> motor_pulse_count = r.read_u32_le();
    if (!motor_pulse_count) return std::nullopt;
    std::optional<uint32_t> tach_pulse_count = r.read_u32_le();
    if (!tach_pulse_count) return std::nullopt;
    std::optional<bool> fault = r.read_bool();
    if (!fault) return std::nullopt;

    Telemetry out;
    out.mcu_tick_us = *mcu_tick_us;
    out.motor_pulse_count = *motor_pulse_count;
    out.tach_pulse_count = *tach_pulse_count;
    out.fault = *fault;
    return out;
}

bool HelloInfo::encode_into(ByteWriter& w) const {
    return w.write_u32_le(firmware_build_id);
}

std::optional<HelloInfo> HelloInfo::decode_from(ByteReader& r) {
    std::optional<uint32_t> firmware_build_id = r.read_u32_le();
    if (!firmware_build_id) return std::nullopt;
    return HelloInfo{*firmware_build_id};
}

bool FromMcu::encode_into(ByteWriter& w) const {
    if (!w.write_u8(static_cast<uint8_t>(payload.index()))) return false;
    return std::visit([&w](const auto& alt) { return alt.encode_into(w); }, payload);
}

std::optional<FromMcu> FromMcu::decode_from(ByteReader& r) {
    std::optional<uint8_t> tag = r.read_u8();
    if (!tag) return std::nullopt;
    switch (*tag) {
        case 0: {
            std::optional<HelloInfo> h = HelloInfo::decode_from(r);
            if (!h) return std::nullopt;
            return make_hello(*h);
        }
        case 1: {
            std::optional<Telemetry> t = Telemetry::decode_from(r);
            if (!t) return std::nullopt;
            return make_telemetry(*t);
        }
        default:
            return std::nullopt;
    }
}

} // namespace protocol
