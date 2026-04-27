from __future__ import annotations

import json
import os
import sqlite3
import threading
from datetime import datetime, timezone
from typing import Any


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


class SQLiteLogStore:
    def __init__(self, path: str) -> None:
        self._path = os.path.abspath(path)
        self._lock = threading.Lock()

        parent = os.path.dirname(self._path)
        if parent:
            os.makedirs(parent, exist_ok=True)

        self._conn = sqlite3.connect(self._path, check_same_thread=False)
        self._conn.execute("PRAGMA journal_mode=WAL")
        self._conn.execute("PRAGMA synchronous=NORMAL")
        self._conn.execute("PRAGMA busy_timeout=5000")
        self._create_schema()

    @classmethod
    def from_env(cls, default_path: str) -> "SQLiteLogStore | None":
        raw = os.getenv("MIKROTOM_SQLITE_PATH", default_path).strip()
        if raw.lower() in {"", "off", "none", "disabled"}:
            return None
        return cls(raw)

    @property
    def path(self) -> str:
        return self._path

    def close(self) -> None:
        with self._lock:
            self._conn.close()

    def _create_schema(self) -> None:
        with self._lock:
            self._conn.executescript(
                """
                CREATE TABLE IF NOT EXISTS protocol_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    recorded_at_utc TEXT NOT NULL,
                    port TEXT,
                    direction TEXT NOT NULL,
                    line TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS telemetry_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    recorded_at_utc TEXT NOT NULL,
                    port TEXT,
                    source_ts_ms INTEGER NOT NULL,
                    timestamp_us INTEGER,
                    position_counts INTEGER,
                    pos_um INTEGER NOT NULL,
                    pos_set_um INTEGER NOT NULL,
                    vel_um_s INTEGER NOT NULL,
                    vel_set_um_s INTEGER NOT NULL,
                    iq_ref_mA INTEGER NOT NULL,
                    iq_meas_mA INTEGER NOT NULL,
                    id_ref_mA INTEGER,
                    id_meas_mA INTEGER,
                    vbus_mV INTEGER,
                    commission_stage INTEGER,
                    status_flags INTEGER,
                    error_flags INTEGER,
                    state INTEGER NOT NULL,
                    fault INTEGER NOT NULL
                );

                CREATE TABLE IF NOT EXISTS event_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    recorded_at_utc TEXT NOT NULL,
                    port TEXT,
                    source_ts_ms INTEGER NOT NULL,
                    code TEXT NOT NULL,
                    value INTEGER NOT NULL
                );

                CREATE TABLE IF NOT EXISTS snapshot_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    recorded_at_utc TEXT NOT NULL,
                    port TEXT,
                    seq INTEGER,
                    stream_id INTEGER NOT NULL,
                    snapshot_kind TEXT NOT NULL,
                    payload_json TEXT NOT NULL
                );

                CREATE INDEX IF NOT EXISTS idx_protocol_log_recorded_at
                    ON protocol_log(recorded_at_utc);
                CREATE INDEX IF NOT EXISTS idx_telemetry_log_source_ts
                    ON telemetry_log(source_ts_ms);
                CREATE INDEX IF NOT EXISTS idx_event_log_source_ts
                    ON event_log(source_ts_ms);
                CREATE INDEX IF NOT EXISTS idx_snapshot_log_recorded_at
                    ON snapshot_log(recorded_at_utc);
                """
            )
            self._ensure_column("telemetry_log", "timestamp_us", "INTEGER")
            self._ensure_column("telemetry_log", "position_counts", "INTEGER")
            self._ensure_column("telemetry_log", "id_ref_mA", "INTEGER")
            self._ensure_column("telemetry_log", "id_meas_mA", "INTEGER")
            self._ensure_column("telemetry_log", "vbus_mV", "INTEGER")
            self._ensure_column("telemetry_log", "commission_stage", "INTEGER")
            self._ensure_column("telemetry_log", "status_flags", "INTEGER")
            self._ensure_column("telemetry_log", "error_flags", "INTEGER")
            self._conn.commit()

    def _ensure_column(self, table_name: str, column_name: str, column_type: str) -> None:
        rows = self._conn.execute(f"PRAGMA table_info({table_name})").fetchall()
        known = {str(row[1]) for row in rows}
        if column_name in known:
            return
        self._conn.execute(f"ALTER TABLE {table_name} ADD COLUMN {column_name} {column_type}")

    def log_protocol(self, port: str, direction: str, line: str) -> None:
        with self._lock:
            self._conn.execute(
                """
                INSERT INTO protocol_log(recorded_at_utc, port, direction, line)
                VALUES (?, ?, ?, ?)
                """,
                (_utc_now_iso(), port, direction, line),
            )
            self._conn.commit()

    def log_telemetry(self, port: str, sample: dict[str, Any]) -> None:
        with self._lock:
            self._conn.execute(
                """
                INSERT INTO telemetry_log(
                    recorded_at_utc,
                    port,
                    source_ts_ms,
                    timestamp_us,
                    position_counts,
                    pos_um,
                    pos_set_um,
                    vel_um_s,
                    vel_set_um_s,
                    iq_ref_mA,
                    iq_meas_mA,
                    id_ref_mA,
                    id_meas_mA,
                    vbus_mV,
                    commission_stage,
                    status_flags,
                    error_flags,
                    state,
                    fault
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    _utc_now_iso(),
                    port,
                    int(sample["ts_ms"]),
                    int(sample.get("timestamp_us", int(sample["ts_ms"]) * 1000)),
                    int(sample.get("position_counts", 0)),
                    int(sample["pos_um"]),
                    int(sample["pos_set_um"]),
                    int(sample["vel_um_s"]),
                    int(sample["vel_set_um_s"]),
                    int(sample["iq_ref_mA"]),
                    int(sample["iq_meas_mA"]),
                    int(sample.get("id_ref_mA", 0)),
                    int(sample.get("id_meas_mA", 0)),
                    int(sample.get("vbus_mV", 0)),
                    int(sample.get("commission_stage", 0)),
                    int(sample.get("status_flags", 0)),
                    int(sample.get("error_flags", 0)),
                    int(sample["state"]),
                    int(sample["fault"]),
                ),
            )
            self._conn.commit()

    def log_event(self, port: str, event: dict[str, Any]) -> None:
        with self._lock:
            self._conn.execute(
                """
                INSERT INTO event_log(recorded_at_utc, port, source_ts_ms, code, value)
                VALUES (?, ?, ?, ?, ?)
                """,
                (
                    _utc_now_iso(),
                    port,
                    int(event["ts_ms"]),
                    str(event["code"]),
                    int(event["value"]),
                ),
            )
            self._conn.commit()

    def log_snapshot(
        self,
        port: str,
        seq: int,
        stream_id: int,
        snapshot_kind: str,
        payload: dict[str, Any],
    ) -> None:
        with self._lock:
            self._conn.execute(
                """
                INSERT INTO snapshot_log(
                    recorded_at_utc,
                    port,
                    seq,
                    stream_id,
                    snapshot_kind,
                    payload_json
                )
                VALUES (?, ?, ?, ?, ?, ?)
                """,
                (
                    _utc_now_iso(),
                    port,
                    int(seq),
                    int(stream_id),
                    str(snapshot_kind),
                    json.dumps(payload, sort_keys=True, separators=(",", ":")),
                ),
            )
            self._conn.commit()

    def _count(self, table_name: str) -> int:
        row = self._conn.execute(f"SELECT COUNT(*) FROM {table_name}").fetchone()
        return int(row[0]) if row is not None else 0

    def stats(self) -> dict[str, Any]:
        with self._lock:
            return {
                "enabled": True,
                "path": self._path,
                "protocol_rows": self._count("protocol_log"),
                "telemetry_rows": self._count("telemetry_log"),
                "event_rows": self._count("event_log"),
                "snapshot_rows": self._count("snapshot_log"),
            }
