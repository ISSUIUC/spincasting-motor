#![cfg_attr(not(test), no_std)]

mod command;
mod telemetry;
mod wire;

pub use command::{ConnectConfig, DeadmanPolicy, DesiredState, Direction, Interval, ToMcu};
pub use telemetry::{FromMcu, HelloInfo, Telemetry};
pub use wire::{decode, encode, WireError, MAX_FRAME_SIZE, MAX_PAYLOAD_SIZE};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip_desired_state() {
        let msg = ToMcu::Command(DesiredState {
            dir: Direction::Clockwise,
            freewheel: false,
            run: true,
            speed: Interval::saturating(0.425),
        });

        let mut payload_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let mut frame = [0u8; MAX_FRAME_SIZE];
        let len = encode(&msg, &mut payload_scratch, &mut frame).unwrap();

        let mut decode_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let decoded: ToMcu = decode(&frame[..len - 1], &mut decode_scratch).unwrap();

        assert_eq!(msg, decoded);
    }

    #[test]
    fn roundtrip_telemetry() {
        let msg = FromMcu::Telemetry(Telemetry {
            mcu_tick_us: 123_456,
            motor_pulse_count: 789,
            tach_pulse_count: 321,
            fault: false,
        });

        let mut payload_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let mut frame = [0u8; MAX_FRAME_SIZE];
        let len = encode(&msg, &mut payload_scratch, &mut frame).unwrap();

        let mut decode_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let decoded: FromMcu = decode(&frame[..len - 1], &mut decode_scratch).unwrap();

        assert_eq!(msg, decoded);
    }

    #[test]
    fn roundtrip_hello() {
        let msg = FromMcu::Hello(HelloInfo {
            firmware_build_id: 0xDEAD_BEEF,
        });

        let mut payload_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let mut frame = [0u8; MAX_FRAME_SIZE];
        let len = encode(&msg, &mut payload_scratch, &mut frame).unwrap();

        let mut decode_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let decoded: FromMcu = decode(&frame[..len - 1], &mut decode_scratch).unwrap();

        assert_eq!(msg, decoded);
    }

    #[test]
    fn connect_config_roundtrip() {
        let msg = ToMcu::Connect(ConnectConfig {
            deadman_policy: DeadmanPolicy::Freewheel,
            deadman_timeout_ms: 1000,
        });

        let mut payload_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let mut frame = [0u8; MAX_FRAME_SIZE];
        let len = encode(&msg, &mut payload_scratch, &mut frame).unwrap();

        let mut decode_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let decoded: ToMcu = decode(&frame[..len - 1], &mut decode_scratch).unwrap();

        assert_eq!(msg, decoded);
    }

    #[test]
    fn disconnect_roundtrip() {
        let msg = ToMcu::Disconnect;

        let mut payload_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let mut frame = [0u8; MAX_FRAME_SIZE];
        let len = encode(&msg, &mut payload_scratch, &mut frame).unwrap();

        let mut decode_scratch = [0u8; MAX_PAYLOAD_SIZE];
        let decoded: ToMcu = decode(&frame[..len - 1], &mut decode_scratch).unwrap();

        assert_eq!(msg, decoded);
    }
}
