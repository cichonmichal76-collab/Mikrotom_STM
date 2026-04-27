from __future__ import annotations

import json
import sqlite3

from agent.log_store import SQLiteLogStore


def test_sqlite_log_store_persists_protocol_telemetry_and_events(tmp_path):
    db_path = tmp_path / "agent.sqlite3"
    store = SQLiteLogStore(str(db_path))

    store.log_protocol("COM_TEST", "tx", "CMD ENABLE")
    store.log_telemetry(
        "COM_TEST",
        {
            "ts_ms": 1,
            "timestamp_us": 1000,
            "position_counts": 11,
            "pos_um": 2,
            "pos_set_um": 3,
            "vel_um_s": 4,
            "vel_set_um_s": 5,
            "iq_ref_mA": 6,
            "iq_meas_mA": 7,
            "id_ref_mA": 8,
            "id_meas_mA": 9,
            "vbus_mV": 12000,
            "commission_stage": 3,
            "status_flags": 0x1234,
            "error_flags": 0x0002,
            "state": 8,
            "fault": 9,
        },
    )
    store.log_event("COM_TEST", {"ts_ms": 10, "code": "BOOT", "value": 0})
    store.log_snapshot(
        "COM_TEST",
        7,
        3,
        "limits",
        {"snapshot_kind": "limits", "max_current_peak_mA": 3000, "soft_min_pos_um": -100},
    )

    assert store.stats()["protocol_rows"] == 1
    assert store.stats()["telemetry_rows"] == 1
    assert store.stats()["event_rows"] == 1
    assert store.stats()["snapshot_rows"] == 1

    store.close()

    conn = sqlite3.connect(db_path)
    try:
        protocol_row = conn.execute(
            "SELECT port, direction, line FROM protocol_log"
        ).fetchone()
        telemetry_row = conn.execute(
            "SELECT port, source_ts_ms, timestamp_us, position_counts, vbus_mV, commission_stage, pos_um, fault FROM telemetry_log"
        ).fetchone()
        event_row = conn.execute(
            "SELECT port, source_ts_ms, code, value FROM event_log"
        ).fetchone()
        snapshot_row = conn.execute(
            "SELECT port, seq, stream_id, snapshot_kind, payload_json FROM snapshot_log"
        ).fetchone()
    finally:
        conn.close()

    assert protocol_row == ("COM_TEST", "tx", "CMD ENABLE")
    assert telemetry_row == ("COM_TEST", 1, 1000, 11, 12000, 3, 2, 9)
    assert event_row == ("COM_TEST", 10, "BOOT", 0)
    assert snapshot_row is not None
    assert snapshot_row[:4] == ("COM_TEST", 7, 3, "limits")
    assert json.loads(snapshot_row[4])["max_current_peak_mA"] == 3000


def test_sqlite_log_store_can_be_disabled_via_env(monkeypatch, tmp_path):
    monkeypatch.setenv("MIKROTOM_SQLITE_PATH", "off")

    store = SQLiteLogStore.from_env(str(tmp_path / "agent.sqlite3"))

    assert store is None
