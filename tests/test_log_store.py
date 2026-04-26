from __future__ import annotations

import sqlite3

from agent.log_store import SQLiteLogStore


def test_sqlite_log_store_persists_sessions_commands_snapshots_and_streams(tmp_path):
    db_path = tmp_path / "agent.sqlite3"
    store = SQLiteLogStore(str(db_path))

    session_id = store.start_session(
        device_id="DEV-01",
        axis_id="ZSS-1",
        operator_id="operator",
        agent_version="0.1.0",
        mode="live",
    )
    build_id = store.ensure_firmware_build(
        fw_name="Mikrotom",
        fw_version="1.2.3",
        build_date="2026-04-26",
        git_hash="abc123",
        safe_integration=0,
        motion_implemented=1,
    )
    store.attach_firmware_build_to_session(session_id, build_id)

    snapshot_id, snapshot_hash = store.log_config_snapshot(
        session_id,
        "status_poll",
        {
            "commissioning_stage": 3,
            "safe_mode": 0,
            "arming_only": 0,
            "controlled_motion": 1,
            "enabled": 1,
            "brake_installed": 1,
            "collision_sensor_installed": 0,
            "ptc_installed": 0,
            "backup_supply_installed": 0,
            "external_interlock_installed": 1,
            "ignore_brake_feedback": 0,
            "ignore_collision_sensor": 0,
            "ignore_external_interlock": 0,
            "allow_motion_without_calibration": 0,
            "calib_valid": 1,
            "calib_zero_pos": 0,
            "calib_pitch_um": 30000.0,
            "calib_sign": 1,
            "max_current": 0.3,
            "max_current_peak": 1.2,
            "max_velocity": 0.005,
            "max_acceleration": 0.02,
            "soft_min_pos": -100,
            "soft_max_pos": 100,
        },
    )
    assert snapshot_hash

    command_id = store.log_command(
        session_id=session_id,
        source="agent_api",
        actor="operator",
        command_type="MOVE_REL",
        raw_command="CMD MOVE_REL 100",
        arg_delta_um=100,
        response_ok=True,
        response_text="OK",
        latency_ms=12,
    )
    store.log_config_change(
        session_id,
        source="apply_params",
        actor="operator",
        parameter_name="max_current",
        old_value=0.2,
        new_value=0.3,
        command_id=command_id,
        snapshot_id_after=snapshot_id,
    )
    motion_run_id = store.start_motion_run(
        session_id=session_id,
        source="agent_api",
        start_command_id=command_id,
        config_snapshot_id=snapshot_id,
        move_type="MOVE_REL",
        requested_target_um=None,
        compensated_target_um=None,
        start_pos_um=0,
    )
    store.log_protocol("COM_TEST", "tx", "CMD MOVE_REL 100", session_id=session_id)
    store.log_telemetry(
        "COM_TEST",
        {
            "ts_ms": 1,
            "pos_um": 2,
            "pos_set_um": 3,
            "vel_um_s": 4,
            "vel_set_um_s": 5,
            "iq_ref_mA": 6,
            "iq_meas_mA": 7,
            "state": 8,
            "fault": 9,
        },
        session_id=session_id,
        motion_run_id=motion_run_id,
        status_context={
            "axis_state": "MOTION",
            "fault": "NONE",
            "run_allowed": 1,
            "enabled": 1,
            "commissioning_stage": 3,
            "vbus_V": 24.0,
        },
    )
    store.log_event(
        "COM_TEST",
        {"ts_ms": 10, "code": "BOOT", "value": 0},
        session_id=session_id,
        motion_run_id=motion_run_id,
    )
    fault_id = store.open_fault_log(
        session_id=session_id,
        motion_run_id=motion_run_id,
        fault_code="FAULT_COMM_TIMEOUT",
        fault_mask=4,
        state_at_set="FAULT",
        source_ts_ms_start=10,
    )
    store.close_fault_log(
        fault_id,
        state_at_clear="SAFE",
        ack_command_id=command_id,
        source_ts_ms_clear=12,
    )
    store.finish_motion_run(
        motion_run_id,
        stop_command_id=command_id,
        end_pos_um=98,
        final_error_um=-2,
        result_status="READY",
        result_detail=None,
    )
    store.end_session(session_id)

    stats = store.stats()
    assert stats["session_rows"] == 1
    assert stats["firmware_build_rows"] == 1
    assert stats["config_snapshot_rows"] == 1
    assert stats["config_change_rows"] == 1
    assert stats["command_rows"] == 1
    assert stats["motion_run_rows"] == 1
    assert stats["protocol_rows"] == 1
    assert stats["telemetry_rows"] == 1
    assert stats["event_rows"] == 1
    assert stats["fault_rows"] == 1

    store.close()

    conn = sqlite3.connect(db_path)
    try:
        session_row = conn.execute(
            "SELECT device_id, axis_id, operator_id, mode, firmware_build_id FROM sessions"
        ).fetchone()
        snapshot_row = conn.execute(
            "SELECT commissioning_stage, max_current_mA, max_velocity_um_s, soft_min_pos_um, soft_max_pos_um FROM config_snapshots"
        ).fetchone()
        command_row = conn.execute(
            "SELECT command_type, raw_command, arg_delta_um, response_ok FROM command_log"
        ).fetchone()
        protocol_row = conn.execute(
            "SELECT channel, direction, line FROM raw_protocol_log"
        ).fetchone()
        telemetry_row = conn.execute(
            "SELECT port, source_ts_ms, pos_um, vbus_mV, axis_state FROM telemetry_log"
        ).fetchone()
        event_row = conn.execute(
            "SELECT port, source_ts_ms, code, value FROM event_log"
        ).fetchone()
        motion_row = conn.execute(
            "SELECT move_type, start_pos_um, end_pos_um, final_error_um, result_status FROM motion_runs"
        ).fetchone()
        fault_row = conn.execute(
            "SELECT fault_code, fault_mask, cleared, state_at_clear FROM fault_log"
        ).fetchone()
    finally:
        conn.close()

    assert session_row[:4] == ("DEV-01", "ZSS-1", "operator", "live")
    assert session_row[4] == 1
    assert snapshot_row == (3, 300, 5000, -100, 100)
    assert command_row == ("MOVE_REL", "CMD MOVE_REL 100", 100, 1)
    assert protocol_row == ("COM_TEST", "tx", "CMD MOVE_REL 100")
    assert telemetry_row == ("COM_TEST", 1, 2, 24000, "MOTION")
    assert event_row == ("COM_TEST", 10, "BOOT", 0)
    assert motion_row == ("MOVE_REL", 0, 98, -2, "READY")
    assert fault_row == ("FAULT_COMM_TIMEOUT", 4, 1, "SAFE")


def test_sqlite_log_store_can_be_disabled_via_env(monkeypatch, tmp_path):
    monkeypatch.setenv("MIKROTOM_SQLITE_PATH", "off")

    store = SQLiteLogStore.from_env(str(tmp_path / "agent.sqlite3"))

    assert store is None
