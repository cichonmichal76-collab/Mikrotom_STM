from __future__ import annotations

import os
import queue
import threading
import time
from collections import deque
from datetime import datetime, timezone
from typing import Any

try:
    import serial
    from serial import SerialException
except ImportError:  # pragma: no cover - handled at runtime on target machine
    serial = None
    SerialException = Exception

from agent.log_store import SQLiteLogStore


class FirmwareError(RuntimeError):
    pass


class FirmwareTransport:
    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        read_timeout_s: float = 0.05,
        command_timeout_s: float = 1.0,
        heartbeat_interval_s: float = 0.35,
        event_cache_size: int = 128,
        log_store: SQLiteLogStore | None = None,
    ) -> None:
        self._port = port
        self._baudrate = baudrate
        self._read_timeout_s = read_timeout_s
        self._command_timeout_s = command_timeout_s
        self._heartbeat_interval_s = heartbeat_interval_s
        self._command_lock = threading.Lock()
        self._connect_lock = threading.Lock()
        self._stop_event = threading.Event()
        self._reader_thread: threading.Thread | None = None
        self._heartbeat_thread: threading.Thread | None = None
        self._serial: Any = None
        self._responses: queue.Queue[str] = queue.Queue()
        self._latest_telemetry: dict[str, int] = self._default_telemetry()
        self._events: deque[dict[str, int | str]] = deque(maxlen=event_cache_size)
        self._last_activity_monotonic = time.monotonic()
        self._log_store = log_store

    @staticmethod
    def _default_telemetry() -> dict[str, int]:
        return {
            "ts_ms": 0,
            "pos_um": 0,
            "pos_set_um": 0,
            "vel_um_s": 0,
            "vel_set_um_s": 0,
            "iq_ref_mA": 0,
            "iq_meas_mA": 0,
            "state": 0,
            "fault": 0,
        }

    @classmethod
    def from_env(cls) -> "FirmwareTransport":
        default_sqlite_path = os.path.join(os.path.dirname(__file__), "mikrotom_agent.sqlite3")
        return cls(
            port=os.getenv("MIKROTOM_SERIAL_PORT", "COM3"),
            baudrate=int(os.getenv("MIKROTOM_SERIAL_BAUDRATE", "115200")),
            read_timeout_s=float(os.getenv("MIKROTOM_SERIAL_READ_TIMEOUT", "0.05")),
            command_timeout_s=float(os.getenv("MIKROTOM_COMMAND_TIMEOUT", "1.0")),
            heartbeat_interval_s=float(os.getenv("MIKROTOM_HEARTBEAT_INTERVAL_S", "0.35")),
            event_cache_size=int(os.getenv("MIKROTOM_EVENT_CACHE_SIZE", "128")),
            log_store=SQLiteLogStore.from_env(default_sqlite_path),
        )

    def close(self) -> None:
        self._stop_event.set()
        with self._connect_lock:
            if self._serial is not None:
                try:
                    self._serial.close()
                except OSError:
                    pass
                self._serial = None

        if self._reader_thread is not None and self._reader_thread.is_alive():
            self._reader_thread.join(timeout=0.5)
        if self._heartbeat_thread is not None and self._heartbeat_thread.is_alive():
            self._heartbeat_thread.join(timeout=0.5)
        if self._log_store is not None:
            self._log_store.close()

    def _start_background_threads(self) -> None:
        if self._reader_thread is None or not self._reader_thread.is_alive():
            self._reader_thread = threading.Thread(
                target=self._reader_loop,
                name="mikrotom-firmware-reader",
                daemon=True,
            )
            self._reader_thread.start()

        if self._heartbeat_interval_s > 0.0 and (self._heartbeat_thread is None or not self._heartbeat_thread.is_alive()):
            self._heartbeat_thread = threading.Thread(
                target=self._heartbeat_loop,
                name="mikrotom-firmware-heartbeat",
                daemon=True,
            )
            self._heartbeat_thread.start()

    def _ensure_connected(self) -> None:
        if serial is None:
            raise FirmwareError("pyserial is not installed")

        with self._connect_lock:
            if self._serial is not None:
                return

            try:
                self._serial = serial.Serial(
                    port=self._port,
                    baudrate=self._baudrate,
                    timeout=self._read_timeout_s,
                )
                self._serial.reset_input_buffer()
                self._serial.reset_output_buffer()
                self._last_activity_monotonic = time.monotonic()
            except SerialException as exc:
                raise FirmwareError(f"serial open failed: {exc}") from exc

            self._stop_event.clear()
            self._start_background_threads()

    def _mark_disconnected(self) -> None:
        with self._connect_lock:
            if self._serial is not None:
                try:
                    self._serial.close()
                except OSError:
                    pass
                self._serial = None

    def _reader_loop(self) -> None:
        while not self._stop_event.is_set():
            try:
                if self._serial is None:
                    return
                raw = self._serial.readline()
            except (OSError, SerialException):
                self._mark_disconnected()
                return

            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            if line.startswith("TEL,"):
                self._log_protocol("rx_tel", line)
                self._handle_telemetry(line)
            elif line.startswith("RSP,"):
                self._log_protocol("rx_rsp", line)
                self._responses.put(line)

    def _heartbeat_loop(self) -> None:
        while not self._stop_event.is_set():
            time.sleep(max(0.05, self._heartbeat_interval_s))

            if self._stop_event.is_set():
                break
            if self._serial is None:
                continue
            if (time.monotonic() - self._last_activity_monotonic) < self._heartbeat_interval_s:
                continue

            try:
                self._send_command("CMD HEARTBEAT")
            except FirmwareError:
                continue

    def _handle_telemetry(self, line: str) -> None:
        parts = line.split(",")
        if len(parts) != 10:
            return

        try:
            telemetry = {
                "ts_ms": int(parts[1]),
                "pos_um": int(parts[2]),
                "pos_set_um": int(parts[3]),
                "vel_um_s": int(parts[4]),
                "vel_set_um_s": int(parts[5]),
                "iq_ref_mA": int(parts[6]),
                "iq_meas_mA": int(parts[7]),
                "state": int(parts[8]),
                "fault": int(parts[9]),
            }
        except ValueError:
            return

        self._latest_telemetry = telemetry
        if self._log_store is not None:
            self._log_store.log_telemetry(self._port, telemetry)

    def _clear_pending_responses(self) -> None:
        while True:
            try:
                self._responses.get_nowait()
            except queue.Empty:
                return

    def _send_line(self, line: str) -> None:
        self._ensure_connected()

        if self._serial is None:
            raise FirmwareError("serial transport is not connected")

        try:
            self._serial.write(f"{line}\n".encode("utf-8"))
            self._serial.flush()
        except (OSError, SerialException) as exc:
            self._mark_disconnected()
            raise FirmwareError(f"serial write failed: {exc}") from exc

        self._log_protocol("tx", line)

    def _send_command(self, line: str) -> dict[str, Any]:
        with self._command_lock:
            self._last_activity_monotonic = time.monotonic()
            self._clear_pending_responses()
            self._send_line(line)

            deadline = time.monotonic() + self._command_timeout_s
            while time.monotonic() < deadline:
                timeout = max(0.01, deadline - time.monotonic())
                try:
                    response = self._responses.get(timeout=timeout)
                except queue.Empty:
                    continue
                self._last_activity_monotonic = time.monotonic()
                return self._parse_response(response)

        raise FirmwareError(f"timeout waiting for response to: {line}")

    @staticmethod
    def _parse_response(line: str) -> dict[str, Any]:
        parts = line.split(",")
        if len(parts) < 2 or parts[0] != "RSP":
            raise FirmwareError(f"invalid response: {line}")

        kind = parts[1]
        if kind == "OK":
            return {"kind": "OK"}
        if kind == "ERR":
            reason = parts[2] if len(parts) > 2 else "UNKNOWN"
            raise FirmwareError(reason)
        if kind == "PARAM":
            return {"kind": "PARAM", "name": parts[2], "value": parts[3]}
        if kind == "CONFIG":
            return {"kind": "CONFIG", "name": parts[2], "value": parts[3]}
        if kind == "STATE":
            return {"kind": "STATE", "value": parts[2]}
        if kind == "FAULT":
            return {"kind": "FAULT", "value": parts[2]}
        if kind == "VERSION":
            return {
                "kind": "VERSION",
                "name": parts[2] if len(parts) > 2 else "",
                "version": parts[3] if len(parts) > 3 else "",
                "build_date": parts[4] if len(parts) > 4 else "",
                "git_hash": parts[5] if len(parts) > 5 else "",
            }
        if kind == "EVENT":
            if len(parts) >= 3 and parts[2] == "NONE":
                return {"kind": "EVENT", "empty": True}
            if len(parts) < 5:
                raise FirmwareError(f"invalid event response: {line}")
            return {
                "kind": "EVENT",
                "empty": False,
                "ts_ms": int(parts[2]),
                "code": parts[3],
                "value": int(parts[4]),
            }

        raise FirmwareError(f"unsupported response: {line}")

    def _read_param_value(self, name: str) -> str:
        return str(self._send_command(f"GET {name}")["value"])

    def _read_param(self, name: str) -> str:
        return str(self._send_command(f"GET PARAM {name}")["value"])

    def _read_config(self, name: str) -> str:
        return str(self._send_command(f"GET CONFIG {name}")["value"])

    def _log_protocol(self, direction: str, line: str) -> None:
        if self._log_store is not None:
            self._log_store.log_protocol(self._port, direction, line)

    def latest_telemetry(self) -> dict[str, int]:
        self._ensure_connected()
        return dict(self._latest_telemetry)

    def drain_events(self, max_events: int = 64) -> list[dict[str, int | str]]:
        drained: list[dict[str, int | str]] = []

        try:
            count = int(self._read_param_value("EVENT_COUNT"))
        except (ValueError, FirmwareError):
            return drained

        for _ in range(min(count, max_events)):
            event = self._send_command("GET EVENT_POP")
            if event.get("empty"):
                break
            normalized = {
                "ts_ms": int(event["ts_ms"]),
                "code": str(event["code"]),
                "value": int(event["value"]),
            }
            self._events.append(normalized)
            if self._log_store is not None:
                self._log_store.log_event(self._port, normalized)
            drained.append(normalized)

        return drained

    def recent_events(self, limit: int = 24) -> list[dict[str, int | str]]:
        self.drain_events()
        events = list(self._events)
        if limit <= 0:
            return events
        return events[-limit:]

    def version_info(self) -> dict[str, str]:
        version = self._send_command("GET VERSION")
        return {
            "name": str(version.get("name", "")),
            "version": str(version.get("version", "")),
            "build_date": str(version.get("build_date", "")),
            "git_hash": str(version.get("git_hash", "")),
        }

    def status(self) -> dict[str, Any]:
        telemetry = self.latest_telemetry()

        return {
            "axis_state": self._send_command("GET STATE")["value"],
            "fault": self._send_command("GET FAULT")["value"],
            "fault_mask": int(self._read_param_value("FAULT_MASK")),
            "commissioning_stage": int(self._read_param_value("COMMISSION_STAGE")),
            "safe_mode": int(self._read_param_value("SAFE_MODE")),
            "arming_only": int(self._read_param_value("ARMING_ONLY")),
            "controlled_motion": int(self._read_param_value("CONTROLLED_MOTION")),
            "run_allowed": int(self._read_param_value("RUN_ALLOWED")),
            "enabled": int(self._read_param_value("ENABLED")),
            "config_loaded": int(self._read_param_value("CONFIG_LOADED")),
            "safe_integration": int(self._read_param_value("SAFE_INTEGRATION")),
            "motion_implemented": int(self._read_param_value("MOTION_IMPLEMENTED")),
            "position_um": int(self._read_param_value("POS")),
            "position_set_um": int(telemetry["pos_set_um"]),
            "brake_installed": int(self._read_config("BRAKE_INSTALLED")),
            "collision_sensor_installed": int(self._read_config("COLLISION_SENSOR_INSTALLED")),
            "ptc_installed": int(self._read_config("PTC_INSTALLED")),
            "backup_supply_installed": int(self._read_config("BACKUP_SUPPLY_INSTALLED")),
            "external_interlock_installed": int(self._read_config("EXTERNAL_INTERLOCK_INSTALLED")),
            "ignore_brake_feedback": int(self._read_config("IGNORE_BRAKE_FEEDBACK")),
            "ignore_collision_sensor": int(self._read_config("IGNORE_COLLISION_SENSOR")),
            "ignore_external_interlock": int(self._read_config("IGNORE_EXTERNAL_INTERLOCK")),
            "allow_motion_without_calibration": int(self._read_config("ALLOW_MOTION_WITHOUT_CALIBRATION")),
            "calib_valid": int(self._read_config("CALIB_VALID")),
            "max_current": float(self._read_param("MAX_CURRENT")),
            "max_current_peak": float(self._read_param("MAX_CURRENT_PEAK")),
            "max_velocity": float(self._read_param("MAX_VELOCITY")),
            "max_acceleration": float(self._read_param("MAX_ACCELERATION")),
            "soft_min_pos": int(self._read_param("SOFT_MIN_POS")),
            "soft_max_pos": int(self._read_param("SOFT_MAX_POS")),
            "calib_zero_pos": int(self._read_param("CALIB_ZERO_POS")),
            "calib_pitch_um": float(self._read_param("CALIB_PITCH_UM")),
            "calib_sign": int(self._read_param("CALIB_SIGN")),
        }

    def debug_vars(self) -> dict[str, Any]:
        status = self.status()
        telemetry = self.latest_telemetry()
        events = self.recent_events(limit=16)

        return {
            "timestamp_utc": datetime.now(timezone.utc).isoformat(),
            "transport": {
                "port": self._port,
                "baudrate": self._baudrate,
                "read_timeout_s": self._read_timeout_s,
                "command_timeout_s": self._command_timeout_s,
                "heartbeat_interval_s": self._heartbeat_interval_s,
                "connected": self._serial is not None,
                "seconds_since_activity": round(time.monotonic() - self._last_activity_monotonic, 3),
                "event_cache_depth": len(self._events),
            },
            "firmware_version": self.version_info(),
            "status": status,
            "telemetry": telemetry,
            "events_recent": events,
            "log_store": self._log_store.stats() if self._log_store is not None else {"enabled": False},
            "protocol_exports": {
                "status_fields": sorted(status.keys()),
                "telemetry_fields": sorted(telemetry.keys()),
                "event_fields": ["ts_ms", "code", "value"],
            },
        }

    def send_ok_command(self, line: str) -> dict[str, bool | str]:
        self._send_command(line)
        return {"ok": True, "command": line}

    def apply_params(self, payload: dict[str, Any]) -> dict[str, Any]:
        command_map = {
            "commissioning_stage": lambda value: f"SET CONFIG COMMISSION_STAGE {int(value)}",
            "safe_mode": lambda value: f"SET CONFIG SAFE_MODE {int(value)}",
            "arming_only": lambda value: f"SET CONFIG ARMING_ONLY {int(value)}",
            "controlled_motion": lambda value: f"SET CONFIG CONTROLLED_MOTION {int(value)}",
            "brake_installed": lambda value: f"SET CONFIG BRAKE_INSTALLED {int(value)}",
            "collision_sensor_installed": lambda value: f"SET CONFIG COLLISION_SENSOR_INSTALLED {int(value)}",
            "ptc_installed": lambda value: f"SET CONFIG PTC_INSTALLED {int(value)}",
            "backup_supply_installed": lambda value: f"SET CONFIG BACKUP_SUPPLY_INSTALLED {int(value)}",
            "external_interlock_installed": lambda value: f"SET CONFIG EXTERNAL_INTERLOCK_INSTALLED {int(value)}",
            "ignore_brake_feedback": lambda value: f"SET CONFIG IGNORE_BRAKE_FEEDBACK {int(value)}",
            "ignore_collision_sensor": lambda value: f"SET CONFIG IGNORE_COLLISION_SENSOR {int(value)}",
            "ignore_external_interlock": lambda value: f"SET CONFIG IGNORE_EXTERNAL_INTERLOCK {int(value)}",
            "allow_motion_without_calibration": lambda value: f"SET CONFIG ALLOW_MOTION_WITHOUT_CALIBRATION {int(value)}",
            "max_current": lambda value: f"SET PARAM MAX_CURRENT {float(value):.6f}",
            "max_current_peak": lambda value: f"SET PARAM MAX_CURRENT_PEAK {float(value):.6f}",
            "max_velocity": lambda value: f"SET PARAM MAX_VELOCITY {float(value):.6f}",
            "max_acceleration": lambda value: f"SET PARAM MAX_ACCELERATION {float(value):.6f}",
            "soft_min_pos": lambda value: f"SET PARAM SOFT_MIN_POS {int(value)}",
            "soft_max_pos": lambda value: f"SET PARAM SOFT_MAX_POS {int(value)}",
            "calib_zero_pos": lambda value: f"SET PARAM CALIB_ZERO_POS {int(value)}",
            "calib_pitch_um": lambda value: f"SET PARAM CALIB_PITCH_UM {float(value):.6f}",
            "calib_sign": lambda value: f"SET PARAM CALIB_SIGN {int(value)}",
        }

        if "calib_valid" in payload and payload["calib_valid"] is not None:
            raise FirmwareError("calib_valid is read-only")

        issued: list[str] = []
        for key, value in payload.items():
            if value is None:
                continue
            if key not in command_map:
                raise FirmwareError(f"unsupported parameter: {key}")
            line = command_map[key](value)
            self._send_command(line)
            issued.append(line)

        return {"ok": True, "commands": issued}
