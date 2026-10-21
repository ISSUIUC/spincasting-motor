//! Generates binary fixtures in to verify both the Rust and C++ implementations
//! decode and re-encode identically

use protocol::{
    encode, ConnectConfig, DeadmanPolicy, DesiredState, Direction, FromMcu, HelloInfo, Interval,
    Telemetry, ToMcu, MAX_FRAME_SIZE, MAX_PAYLOAD_SIZE,
};
use std::fs;
use std::path::Path;

fn write_fixture<T: protocol::WireEncode>(dir: &Path, name: &str, value: &T) {
    let mut payload_scratch = [0u8; MAX_PAYLOAD_SIZE];
    let mut frame = [0u8; MAX_FRAME_SIZE];
    let len = encode(value, &mut payload_scratch, &mut frame).expect("encode fixture");
    fs::write(dir.join(name), &frame[..len]).expect("write fixture");
    println!("{name}: {len} bytes");
}

fn main() {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../protocol-fixtures");
    fs::create_dir_all(&dir).expect("create fixtures dir");

    write_fixture(
        &dir,
        "connect_hold_250.bin",
        &ToMcu::Connect(ConnectConfig {
            deadman_policy: DeadmanPolicy::Hold,
            deadman_timeout_ms: 250,
        }),
    );

    write_fixture(&dir, "disconnect.bin", &ToMcu::Disconnect);

    write_fixture(
        &dir,
        "command_zero_ccw_freewheel.bin",
        &ToMcu::Command(DesiredState {
            speed: Interval::saturating(0.0),
            dir: Direction::CounterClockwise,
            freewheel: true,
            run: false,
        }),
    );

    write_fixture(
        &dir,
        "command_full_cw_run.bin",
        &ToMcu::Command(DesiredState {
            speed: Interval::saturating(1.0),
            dir: Direction::Clockwise,
            freewheel: false,
            run: true,
        }),
    );

    write_fixture(
        &dir,
        "hello_build_12345678.bin",
        &FromMcu::Hello(HelloInfo {
            firmware_build_id: 0x1234_5678,
        }),
    );

    write_fixture(
        &dir,
        "telemetry_near_wrap_fault.bin",
        &FromMcu::Telemetry(Telemetry {
            mcu_tick_us: 4_000_000_000,
            motor_pulse_count: 1,
            tach_pulse_count: 0,
            fault: true,
        }),
    );
}
