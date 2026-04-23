from __future__ import annotations

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
            "pos_um": 2,
            "pos_set_um": 3,
            "vel_um_s": 4,
            "vel_set_um_s": 5,
            "iq_ref_mA": 6,
            "iq_meas_mA": 7,
            "state": 8,
            "fault": 9,
        },
    )
    store.log_event("COM_TEST", {"ts_ms": 10, "code": "BOOT", "value": 0})

    assert store.stats()["protocol_rows"] == 1
    assert store.stats()["telemetry_rows"] == 1
    assert store.stats()["event_rows"] == 1

    store.close()

    conn = sqlite3.connect(db_path)
    try:
        protocol_row = conn.execute(
            "SELECT port, direction, line FROM protocol_log"
        ).fetchone()
        telemetry_row = conn.execute(
            "SELECT port, source_ts_ms, pos_um, fault FROM telemetry_log"
        ).fetchone()
        event_row = conn.execute(
            "SELECT port, source_ts_ms, code, value FROM event_log"
        ).fetchone()
    finally:
        conn.close()

    assert protocol_row == ("COM_TEST", "tx", "CMD ENABLE")
    assert telemetry_row == ("COM_TEST", 1, 2, 9)
    assert event_row == ("COM_TEST", 10, "BOOT", 0)


def test_sqlite_log_store_can_be_disabled_via_env(monkeypatch, tmp_path):
    monkeypatch.setenv("MIKROTOM_SQLITE_PATH", "off")

    store = SQLiteLogStore.from_env(str(tmp_path / "agent.sqlite3"))

    assert store is None
