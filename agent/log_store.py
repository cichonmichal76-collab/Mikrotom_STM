from __future__ import annotations

import hashlib
import json
import os
import sqlite3
import threading
from datetime import datetime, timezone
from typing import Any


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _scaled_int(value: Any, scale: float = 1.0) -> int | None:
    if value is None:
        return None
    return int(round(float(value) * scale))


def _infer_value_type(value: Any) -> str:
    if isinstance(value, bool):
        return "bool"
    if isinstance(value, int):
        return "int"
    if isinstance(value, float):
        return "float"
    if value is None:
        return "null"
    return "text"


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
                CREATE TABLE IF NOT EXISTS schema_info (
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL
                );

                INSERT OR REPLACE INTO schema_info(key, value)
                VALUES ('schema_version', '2');

                CREATE TABLE IF NOT EXISTS firmware_builds (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    fw_name TEXT NOT NULL,
                    fw_version TEXT NOT NULL,
                    build_date TEXT,
                    git_hash TEXT,
                    safe_integration INTEGER,
                    motion_implemented INTEGER,
                    created_at_utc TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS sessions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    started_at_utc TEXT NOT NULL,
                    ended_at_utc TEXT,
                    device_id TEXT,
                    axis_id TEXT,
                    operator_id TEXT,
                    gui_version TEXT,
                    agent_version TEXT,
                    hmi_version TEXT,
                    firmware_build_id INTEGER,
                    mode TEXT NOT NULL,
                    notes TEXT,
                    FOREIGN KEY (firmware_build_id) REFERENCES firmware_builds(id)
                );

                CREATE TABLE IF NOT EXISTS config_snapshots (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER NOT NULL,
                    recorded_at_utc TEXT NOT NULL,
                    source TEXT NOT NULL,
                    commissioning_stage INTEGER,
                    safe_mode INTEGER,
                    arming_only INTEGER,
                    controlled_motion INTEGER,
                    enabled INTEGER,
                    brake_installed INTEGER,
                    collision_sensor_installed INTEGER,
                    ptc_installed INTEGER,
                    backup_supply_installed INTEGER,
                    external_interlock_installed INTEGER,
                    ignore_brake_feedback INTEGER,
                    ignore_collision_sensor INTEGER,
                    ignore_external_interlock INTEGER,
                    allow_motion_without_calibration INTEGER,
                    calib_valid INTEGER,
                    calib_zero_pos_um INTEGER,
                    calib_pitch_um REAL,
                    calib_sign INTEGER,
                    max_current_mA INTEGER,
                    max_current_peak_mA INTEGER,
                    max_velocity_um_s INTEGER,
                    max_acceleration_um_s2 INTEGER,
                    soft_min_pos_um INTEGER,
                    soft_max_pos_um INTEGER,
                    pos_pid_kp REAL,
                    pos_pid_ki REAL,
                    pos_pid_kd REAL,
                    vel_pid_kp REAL,
                    vel_pid_ki REAL,
                    vel_pid_kd REAL,
                    curr_pid_kp REAL,
                    curr_pid_ki REAL,
                    curr_pid_kd REAL,
                    comp_profile_id TEXT,
                    comp_enabled INTEGER,
                    comp_version TEXT,
                    snapshot_hash TEXT NOT NULL,
                    FOREIGN KEY (session_id) REFERENCES sessions(id)
                );

                CREATE TABLE IF NOT EXISTS config_changes (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER NOT NULL,
                    recorded_at_utc TEXT NOT NULL,
                    source TEXT NOT NULL,
                    actor TEXT,
                    parameter_name TEXT NOT NULL,
                    old_value_text TEXT,
                    new_value_text TEXT NOT NULL,
                    value_type TEXT NOT NULL,
                    command_id INTEGER,
                    snapshot_id_after INTEGER,
                    FOREIGN KEY (session_id) REFERENCES sessions(id),
                    FOREIGN KEY (snapshot_id_after) REFERENCES config_snapshots(id)
                );

                CREATE TABLE IF NOT EXISTS command_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER,
                    recorded_at_utc TEXT NOT NULL,
                    source TEXT NOT NULL,
                    actor TEXT,
                    command_type TEXT NOT NULL,
                    raw_command TEXT NOT NULL,
                    arg_target_um INTEGER,
                    arg_delta_um INTEGER,
                    arg_value_text TEXT,
                    response_ok INTEGER,
                    response_text TEXT,
                    latency_ms INTEGER,
                    motion_run_id INTEGER,
                    FOREIGN KEY (session_id) REFERENCES sessions(id)
                );

                CREATE TABLE IF NOT EXISTS motion_runs (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER NOT NULL,
                    started_at_utc TEXT NOT NULL,
                    ended_at_utc TEXT,
                    source TEXT NOT NULL,
                    start_command_id INTEGER,
                    stop_command_id INTEGER,
                    config_snapshot_id INTEGER,
                    move_type TEXT NOT NULL,
                    requested_target_um INTEGER,
                    compensated_target_um INTEGER,
                    start_pos_um INTEGER,
                    end_pos_um INTEGER,
                    final_error_um INTEGER,
                    peak_abs_error_um INTEGER,
                    overshoot_um INTEGER,
                    settle_time_ms INTEGER,
                    peak_vel_um_s INTEGER,
                    peak_iq_ref_mA INTEGER,
                    peak_iq_meas_mA INTEGER,
                    result_status TEXT NOT NULL,
                    result_detail TEXT,
                    FOREIGN KEY (session_id) REFERENCES sessions(id),
                    FOREIGN KEY (config_snapshot_id) REFERENCES config_snapshots(id)
                );

                CREATE TABLE IF NOT EXISTS telemetry_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER,
                    motion_run_id INTEGER,
                    recorded_at_utc TEXT NOT NULL,
                    port TEXT,
                    source_ts_ms INTEGER NOT NULL,
                    pos_um INTEGER NOT NULL,
                    pos_set_um INTEGER NOT NULL,
                    vel_um_s INTEGER NOT NULL,
                    vel_set_um_s INTEGER NOT NULL,
                    acc_set_um_s2 INTEGER,
                    iq_ref_mA INTEGER NOT NULL,
                    iq_meas_mA INTEGER NOT NULL,
                    vbus_mV INTEGER,
                    temperature_mC INTEGER,
                    axis_state TEXT,
                    fault_code TEXT,
                    run_allowed INTEGER,
                    enabled INTEGER,
                    commissioning_stage INTEGER,
                    FOREIGN KEY (session_id) REFERENCES sessions(id),
                    FOREIGN KEY (motion_run_id) REFERENCES motion_runs(id)
                );

                CREATE TABLE IF NOT EXISTS event_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER,
                    motion_run_id INTEGER,
                    recorded_at_utc TEXT NOT NULL,
                    port TEXT,
                    source_ts_ms INTEGER NOT NULL,
                    code TEXT NOT NULL,
                    value INTEGER NOT NULL,
                    detail_text TEXT,
                    FOREIGN KEY (session_id) REFERENCES sessions(id),
                    FOREIGN KEY (motion_run_id) REFERENCES motion_runs(id)
                );

                CREATE TABLE IF NOT EXISTS fault_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER NOT NULL,
                    motion_run_id INTEGER,
                    started_at_utc TEXT NOT NULL,
                    cleared_at_utc TEXT,
                    source_ts_ms_start INTEGER,
                    source_ts_ms_clear INTEGER,
                    fault_code TEXT NOT NULL,
                    fault_mask INTEGER,
                    state_at_set TEXT,
                    state_at_clear TEXT,
                    ack_command_id INTEGER,
                    cleared INTEGER NOT NULL DEFAULT 0,
                    FOREIGN KEY (session_id) REFERENCES sessions(id),
                    FOREIGN KEY (motion_run_id) REFERENCES motion_runs(id)
                );

                CREATE TABLE IF NOT EXISTS calibration_runs (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER NOT NULL,
                    started_at_utc TEXT NOT NULL,
                    ended_at_utc TEXT,
                    source TEXT NOT NULL,
                    trigger_command_id INTEGER,
                    success INTEGER NOT NULL,
                    zero_before_um INTEGER,
                    zero_after_um INTEGER,
                    pitch_before_um REAL,
                    pitch_after_um REAL,
                    sign_before INTEGER,
                    sign_after INTEGER,
                    left_endstop_um INTEGER,
                    right_endstop_um INTEGER,
                    notes TEXT,
                    FOREIGN KEY (session_id) REFERENCES sessions(id)
                );

                CREATE TABLE IF NOT EXISTS raw_protocol_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER,
                    recorded_at_utc TEXT NOT NULL,
                    direction TEXT NOT NULL,
                    channel TEXT NOT NULL,
                    line TEXT NOT NULL,
                    FOREIGN KEY (session_id) REFERENCES sessions(id)
                );

                CREATE INDEX IF NOT EXISTS idx_sessions_started
                    ON sessions(started_at_utc);
                CREATE INDEX IF NOT EXISTS idx_cfg_snapshots_session_time
                    ON config_snapshots(session_id, recorded_at_utc);
                CREATE INDEX IF NOT EXISTS idx_cfg_changes_session_param_time
                    ON config_changes(session_id, parameter_name, recorded_at_utc);
                CREATE INDEX IF NOT EXISTS idx_commands_session_time
                    ON command_log(session_id, recorded_at_utc);
                CREATE INDEX IF NOT EXISTS idx_motion_runs_session_time
                    ON motion_runs(session_id, started_at_utc);
                CREATE INDEX IF NOT EXISTS idx_telemetry_session_ts
                    ON telemetry_log(session_id, source_ts_ms);
                CREATE INDEX IF NOT EXISTS idx_telemetry_motion_ts
                    ON telemetry_log(motion_run_id, source_ts_ms);
                CREATE INDEX IF NOT EXISTS idx_events_session_ts
                    ON event_log(session_id, source_ts_ms);
                CREATE INDEX IF NOT EXISTS idx_faults_session_time
                    ON fault_log(session_id, started_at_utc);
                CREATE INDEX IF NOT EXISTS idx_raw_protocol_session_time
                    ON raw_protocol_log(session_id, recorded_at_utc);
                """
            )
            self._conn.commit()

    def start_session(
        self,
        *,
        device_id: str | None = None,
        axis_id: str | None = None,
        operator_id: str | None = None,
        gui_version: str | None = None,
        agent_version: str | None = None,
        hmi_version: str | None = None,
        mode: str = "live",
        notes: str | None = None,
    ) -> int:
        with self._lock:
            cursor = self._conn.execute(
                """
                INSERT INTO sessions(
                    started_at_utc,
                    device_id,
                    axis_id,
                    operator_id,
                    gui_version,
                    agent_version,
                    hmi_version,
                    mode,
                    notes
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    _utc_now_iso(),
                    device_id,
                    axis_id,
                    operator_id,
                    gui_version,
                    agent_version,
                    hmi_version,
                    mode,
                    notes,
                ),
            )
            self._conn.commit()
            return int(cursor.lastrowid)

    def end_session(self, session_id: int) -> None:
        with self._lock:
            self._conn.execute(
                "UPDATE sessions SET ended_at_utc = ? WHERE id = ?",
                (_utc_now_iso(), int(session_id)),
            )
            self._conn.commit()

    def ensure_firmware_build(
        self,
        *,
        fw_name: str,
        fw_version: str,
        build_date: str | None,
        git_hash: str | None,
        safe_integration: int | None = None,
        motion_implemented: int | None = None,
    ) -> int:
        with self._lock:
            row = self._conn.execute(
                """
                SELECT id, safe_integration, motion_implemented
                FROM firmware_builds
                WHERE fw_name = ?
                  AND fw_version = ?
                  AND IFNULL(build_date, '') = IFNULL(?, '')
                  AND IFNULL(git_hash, '') = IFNULL(?, '')
                ORDER BY id DESC
                LIMIT 1
                """,
                (fw_name, fw_version, build_date, git_hash),
            ).fetchone()

            if row is not None:
                build_id = int(row[0])
                current_safe = row[1]
                current_motion = row[2]
                if safe_integration != current_safe or motion_implemented != current_motion:
                    self._conn.execute(
                        """
                        UPDATE firmware_builds
                        SET safe_integration = ?,
                            motion_implemented = ?
                        WHERE id = ?
                        """,
                        (safe_integration, motion_implemented, build_id),
                    )
                    self._conn.commit()
                return build_id

            cursor = self._conn.execute(
                """
                INSERT INTO firmware_builds(
                    fw_name,
                    fw_version,
                    build_date,
                    git_hash,
                    safe_integration,
                    motion_implemented,
                    created_at_utc
                )
                VALUES (?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    fw_name,
                    fw_version,
                    build_date,
                    git_hash,
                    safe_integration,
                    motion_implemented,
                    _utc_now_iso(),
                ),
            )
            self._conn.commit()
            return int(cursor.lastrowid)

    def attach_firmware_build_to_session(self, session_id: int, firmware_build_id: int) -> None:
        with self._lock:
            self._conn.execute(
                "UPDATE sessions SET firmware_build_id = ? WHERE id = ?",
                (int(firmware_build_id), int(session_id)),
            )
            self._conn.commit()

    def _config_snapshot_payload(self, snapshot: dict[str, Any]) -> dict[str, Any]:
        payload = {
            "commissioning_stage": snapshot.get("commissioning_stage"),
            "safe_mode": snapshot.get("safe_mode"),
            "arming_only": snapshot.get("arming_only"),
            "controlled_motion": snapshot.get("controlled_motion"),
            "enabled": snapshot.get("enabled"),
            "brake_installed": snapshot.get("brake_installed"),
            "collision_sensor_installed": snapshot.get("collision_sensor_installed"),
            "ptc_installed": snapshot.get("ptc_installed"),
            "backup_supply_installed": snapshot.get("backup_supply_installed"),
            "external_interlock_installed": snapshot.get("external_interlock_installed"),
            "ignore_brake_feedback": snapshot.get("ignore_brake_feedback"),
            "ignore_collision_sensor": snapshot.get("ignore_collision_sensor"),
            "ignore_external_interlock": snapshot.get("ignore_external_interlock"),
            "allow_motion_without_calibration": snapshot.get("allow_motion_without_calibration"),
            "calib_valid": snapshot.get("calib_valid"),
            "calib_zero_pos_um": snapshot.get("calib_zero_pos"),
            "calib_pitch_um": snapshot.get("calib_pitch_um"),
            "calib_sign": snapshot.get("calib_sign"),
            "max_current_mA": _scaled_int(snapshot.get("max_current"), 1000.0),
            "max_current_peak_mA": _scaled_int(snapshot.get("max_current_peak"), 1000.0),
            "max_velocity_um_s": _scaled_int(snapshot.get("max_velocity"), 1000000.0),
            "max_acceleration_um_s2": _scaled_int(snapshot.get("max_acceleration"), 1000000.0),
            "soft_min_pos_um": snapshot.get("soft_min_pos"),
            "soft_max_pos_um": snapshot.get("soft_max_pos"),
            "pos_pid_kp": snapshot.get("pos_pid_kp"),
            "pos_pid_ki": snapshot.get("pos_pid_ki"),
            "pos_pid_kd": snapshot.get("pos_pid_kd"),
            "vel_pid_kp": snapshot.get("vel_pid_kp"),
            "vel_pid_ki": snapshot.get("vel_pid_ki"),
            "vel_pid_kd": snapshot.get("vel_pid_kd"),
            "curr_pid_kp": snapshot.get("curr_pid_kp"),
            "curr_pid_ki": snapshot.get("curr_pid_ki"),
            "curr_pid_kd": snapshot.get("curr_pid_kd"),
            "comp_profile_id": snapshot.get("comp_profile_id"),
            "comp_enabled": snapshot.get("comp_enabled"),
            "comp_version": snapshot.get("comp_version"),
        }
        return payload

    def snapshot_hash(self, snapshot: dict[str, Any]) -> str:
        payload = self._config_snapshot_payload(snapshot)
        encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True)
        return hashlib.sha256(encoded.encode("utf-8")).hexdigest()

    def log_config_snapshot(
        self,
        session_id: int,
        source: str,
        snapshot: dict[str, Any],
    ) -> tuple[int, str]:
        payload = self._config_snapshot_payload(snapshot)
        snapshot_hash = self.snapshot_hash(snapshot)

        with self._lock:
            cursor = self._conn.execute(
                """
                INSERT INTO config_snapshots(
                    session_id,
                    recorded_at_utc,
                    source,
                    commissioning_stage,
                    safe_mode,
                    arming_only,
                    controlled_motion,
                    enabled,
                    brake_installed,
                    collision_sensor_installed,
                    ptc_installed,
                    backup_supply_installed,
                    external_interlock_installed,
                    ignore_brake_feedback,
                    ignore_collision_sensor,
                    ignore_external_interlock,
                    allow_motion_without_calibration,
                    calib_valid,
                    calib_zero_pos_um,
                    calib_pitch_um,
                    calib_sign,
                    max_current_mA,
                    max_current_peak_mA,
                    max_velocity_um_s,
                    max_acceleration_um_s2,
                    soft_min_pos_um,
                    soft_max_pos_um,
                    pos_pid_kp,
                    pos_pid_ki,
                    pos_pid_kd,
                    vel_pid_kp,
                    vel_pid_ki,
                    vel_pid_kd,
                    curr_pid_kp,
                    curr_pid_ki,
                    curr_pid_kd,
                    comp_profile_id,
                    comp_enabled,
                    comp_version,
                    snapshot_hash
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    int(session_id),
                    _utc_now_iso(),
                    source,
                    payload["commissioning_stage"],
                    payload["safe_mode"],
                    payload["arming_only"],
                    payload["controlled_motion"],
                    payload["enabled"],
                    payload["brake_installed"],
                    payload["collision_sensor_installed"],
                    payload["ptc_installed"],
                    payload["backup_supply_installed"],
                    payload["external_interlock_installed"],
                    payload["ignore_brake_feedback"],
                    payload["ignore_collision_sensor"],
                    payload["ignore_external_interlock"],
                    payload["allow_motion_without_calibration"],
                    payload["calib_valid"],
                    payload["calib_zero_pos_um"],
                    payload["calib_pitch_um"],
                    payload["calib_sign"],
                    payload["max_current_mA"],
                    payload["max_current_peak_mA"],
                    payload["max_velocity_um_s"],
                    payload["max_acceleration_um_s2"],
                    payload["soft_min_pos_um"],
                    payload["soft_max_pos_um"],
                    payload["pos_pid_kp"],
                    payload["pos_pid_ki"],
                    payload["pos_pid_kd"],
                    payload["vel_pid_kp"],
                    payload["vel_pid_ki"],
                    payload["vel_pid_kd"],
                    payload["curr_pid_kp"],
                    payload["curr_pid_ki"],
                    payload["curr_pid_kd"],
                    payload["comp_profile_id"],
                    payload["comp_enabled"],
                    payload["comp_version"],
                    snapshot_hash,
                ),
            )
            self._conn.commit()
            return int(cursor.lastrowid), snapshot_hash

    def log_config_change(
        self,
        session_id: int,
        *,
        source: str,
        actor: str | None,
        parameter_name: str,
        old_value: Any,
        new_value: Any,
        command_id: int | None = None,
        snapshot_id_after: int | None = None,
    ) -> int:
        with self._lock:
            cursor = self._conn.execute(
                """
                INSERT INTO config_changes(
                    session_id,
                    recorded_at_utc,
                    source,
                    actor,
                    parameter_name,
                    old_value_text,
                    new_value_text,
                    value_type,
                    command_id,
                    snapshot_id_after
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    int(session_id),
                    _utc_now_iso(),
                    source,
                    actor,
                    parameter_name,
                    None if old_value is None else str(old_value),
                    str(new_value),
                    _infer_value_type(new_value),
                    command_id,
                    snapshot_id_after,
                ),
            )
            self._conn.commit()
            return int(cursor.lastrowid)

    def log_command(
        self,
        *,
        session_id: int | None,
        source: str,
        actor: str | None,
        command_type: str,
        raw_command: str,
        arg_target_um: int | None = None,
        arg_delta_um: int | None = None,
        arg_value_text: str | None = None,
        response_ok: bool | None = None,
        response_text: str | None = None,
        latency_ms: int | None = None,
        motion_run_id: int | None = None,
    ) -> int:
        with self._lock:
            cursor = self._conn.execute(
                """
                INSERT INTO command_log(
                    session_id,
                    recorded_at_utc,
                    source,
                    actor,
                    command_type,
                    raw_command,
                    arg_target_um,
                    arg_delta_um,
                    arg_value_text,
                    response_ok,
                    response_text,
                    latency_ms,
                    motion_run_id
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    session_id,
                    _utc_now_iso(),
                    source,
                    actor,
                    command_type,
                    raw_command,
                    arg_target_um,
                    arg_delta_um,
                    arg_value_text,
                    None if response_ok is None else int(bool(response_ok)),
                    response_text,
                    latency_ms,
                    motion_run_id,
                ),
            )
            self._conn.commit()
            return int(cursor.lastrowid)

    def start_motion_run(
        self,
        *,
        session_id: int,
        source: str,
        start_command_id: int | None,
        config_snapshot_id: int | None,
        move_type: str,
        requested_target_um: int | None,
        compensated_target_um: int | None,
        start_pos_um: int | None,
    ) -> int:
        with self._lock:
            cursor = self._conn.execute(
                """
                INSERT INTO motion_runs(
                    session_id,
                    started_at_utc,
                    source,
                    start_command_id,
                    config_snapshot_id,
                    move_type,
                    requested_target_um,
                    compensated_target_um,
                    start_pos_um,
                    result_status
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    int(session_id),
                    _utc_now_iso(),
                    source,
                    start_command_id,
                    config_snapshot_id,
                    move_type,
                    requested_target_um,
                    compensated_target_um,
                    start_pos_um,
                    "ACTIVE",
                ),
            )
            self._conn.commit()
            return int(cursor.lastrowid)

    def finish_motion_run(
        self,
        motion_run_id: int,
        *,
        stop_command_id: int | None = None,
        end_pos_um: int | None = None,
        final_error_um: int | None = None,
        result_status: str,
        result_detail: str | None = None,
    ) -> None:
        with self._lock:
            self._conn.execute(
                """
                UPDATE motion_runs
                SET ended_at_utc = ?,
                    stop_command_id = ?,
                    end_pos_um = ?,
                    final_error_um = ?,
                    result_status = ?,
                    result_detail = ?
                WHERE id = ?
                """,
                (
                    _utc_now_iso(),
                    stop_command_id,
                    end_pos_um,
                    final_error_um,
                    result_status,
                    result_detail,
                    int(motion_run_id),
                ),
            )
            self._conn.commit()

    def open_fault_log(
        self,
        *,
        session_id: int,
        motion_run_id: int | None,
        fault_code: str,
        fault_mask: int | None,
        state_at_set: str | None,
        source_ts_ms_start: int | None = None,
    ) -> int:
        with self._lock:
            cursor = self._conn.execute(
                """
                INSERT INTO fault_log(
                    session_id,
                    motion_run_id,
                    started_at_utc,
                    source_ts_ms_start,
                    fault_code,
                    fault_mask,
                    state_at_set
                )
                VALUES (?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    int(session_id),
                    motion_run_id,
                    _utc_now_iso(),
                    source_ts_ms_start,
                    fault_code,
                    fault_mask,
                    state_at_set,
                ),
            )
            self._conn.commit()
            return int(cursor.lastrowid)

    def close_fault_log(
        self,
        fault_log_id: int,
        *,
        state_at_clear: str | None,
        ack_command_id: int | None = None,
        source_ts_ms_clear: int | None = None,
    ) -> None:
        with self._lock:
            self._conn.execute(
                """
                UPDATE fault_log
                SET cleared_at_utc = ?,
                    source_ts_ms_clear = ?,
                    state_at_clear = ?,
                    ack_command_id = ?,
                    cleared = 1
                WHERE id = ?
                """,
                (
                    _utc_now_iso(),
                    source_ts_ms_clear,
                    state_at_clear,
                    ack_command_id,
                    int(fault_log_id),
                ),
            )
            self._conn.commit()

    def log_protocol(
        self,
        port: str,
        direction: str,
        line: str,
        *,
        session_id: int | None = None,
        channel: str | None = None,
    ) -> None:
        with self._lock:
            self._conn.execute(
                """
                INSERT INTO raw_protocol_log(session_id, recorded_at_utc, direction, channel, line)
                VALUES (?, ?, ?, ?, ?)
                """,
                (
                    session_id,
                    _utc_now_iso(),
                    direction,
                    channel or port,
                    line,
                ),
            )
            self._conn.commit()

    def log_telemetry(
        self,
        port: str,
        sample: dict[str, Any],
        *,
        session_id: int | None = None,
        motion_run_id: int | None = None,
        status_context: dict[str, Any] | None = None,
    ) -> None:
        context = status_context or {}
        with self._lock:
            self._conn.execute(
                """
                INSERT INTO telemetry_log(
                    session_id,
                    motion_run_id,
                    recorded_at_utc,
                    port,
                    source_ts_ms,
                    pos_um,
                    pos_set_um,
                    vel_um_s,
                    vel_set_um_s,
                    acc_set_um_s2,
                    iq_ref_mA,
                    iq_meas_mA,
                    vbus_mV,
                    temperature_mC,
                    axis_state,
                    fault_code,
                    run_allowed,
                    enabled,
                    commissioning_stage
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    session_id,
                    motion_run_id,
                    _utc_now_iso(),
                    port,
                    int(sample["ts_ms"]),
                    int(sample["pos_um"]),
                    int(sample["pos_set_um"]),
                    int(sample["vel_um_s"]),
                    int(sample["vel_set_um_s"]),
                    _scaled_int(sample.get("acc_set_um_s2"), 1.0),
                    int(sample["iq_ref_mA"]),
                    int(sample["iq_meas_mA"]),
                    _scaled_int(context.get("vbus_V"), 1000.0),
                    _scaled_int(context.get("temperature_C"), 1000.0),
                    None if context.get("axis_state") is None else str(context.get("axis_state")),
                    None if context.get("fault") is None else str(context.get("fault")),
                    context.get("run_allowed"),
                    context.get("enabled"),
                    context.get("commissioning_stage"),
                ),
            )
            self._conn.commit()

    def log_event(
        self,
        port: str,
        event: dict[str, Any],
        *,
        session_id: int | None = None,
        motion_run_id: int | None = None,
        detail_text: str | None = None,
    ) -> None:
        with self._lock:
            self._conn.execute(
                """
                INSERT INTO event_log(
                    session_id,
                    motion_run_id,
                    recorded_at_utc,
                    port,
                    source_ts_ms,
                    code,
                    value,
                    detail_text
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    session_id,
                    motion_run_id,
                    _utc_now_iso(),
                    port,
                    int(event["ts_ms"]),
                    str(event["code"]),
                    int(event["value"]),
                    detail_text,
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
                "session_rows": self._count("sessions"),
                "firmware_build_rows": self._count("firmware_builds"),
                "config_snapshot_rows": self._count("config_snapshots"),
                "config_change_rows": self._count("config_changes"),
                "command_rows": self._count("command_log"),
                "motion_run_rows": self._count("motion_runs"),
                "protocol_rows": self._count("raw_protocol_log"),
                "telemetry_rows": self._count("telemetry_log"),
                "event_rows": self._count("event_log"),
                "fault_rows": self._count("fault_log"),
            }
