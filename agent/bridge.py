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
    SOF1 = 0xAA
    SOF2 = 0x55
    FRAME_TYPE_TELEMETRY = 0x01
    FRAME_TYPE_COMMAND = 0x02
    FRAME_TYPE_RESPONSE = 0x03
    FRAME_TYPE_ERROR = 0x04
    FRAME_TYPE_HEARTBEAT = 0x05
    STREAM_ID_FAST_MOTION = 0x01
    STREAM_ID_RUNTIME_STATUS = 0x02
    STREAM_ID_LIMITS = 0x03
    STREAM_ID_SAFETY_CONFIG = 0x04
    STREAM_ID_CALIBRATION = 0x05
    STREAM_ID_EVENT = 0x06
    STREAM_ID_VERSION = 0x07
    REQUEST_RUNTIME_STATUS = 1 << 0
    REQUEST_LIMITS = 1 << 1
    REQUEST_SAFETY_CONFIG = 1 << 2
    REQUEST_CALIBRATION = 1 << 3
    REQUEST_VERSION = 1 << 4
    PROTOCOL_MODE_ASCII = "ascii"
    PROTOCOL_MODE_BINARY = "binary"
    PROTOCOL_MODE_AUTO = "auto"
    CMD_MOVE_RELATIVE = 0x02
    CMD_MOVE_ABSOLUTE = 0x03
    CMD_STOP = 0x04
    CMD_RESET_FAULT = 0x05
    CMD_REQUEST_STATUS = 0x06
    CMD_ENABLE = 0x07
    CMD_DISABLE = 0x08
    CMD_CALIBRATION_ZERO = 0x0B
    RSP_ACK = 0x01
    RSP_NACK = 0x02
    RSP_SNAPSHOT = 0x03
    ERR_CRC_FAIL = 0x01
    ERR_LEN_FAIL = 0x02
    ERR_UNKNOWN_TYPE = 0x03
    ERR_UNKNOWN_COMMAND = 0x04
    ERR_STATE_REJECTED = 0x06
    ERR_BUSY = 0x08
    HEARTBEAT_PC_TO_MCU = 0x01
    HEARTBEAT_MCU_TO_PC = 0x02

    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        read_timeout_s: float = 0.05,
        command_timeout_s: float = 1.0,
        heartbeat_interval_s: float = 0.35,
        event_cache_size: int = 128,
        log_store: SQLiteLogStore | None = None,
        protocol_mode: str = PROTOCOL_MODE_AUTO,
    ) -> None:
        self._port = port
        self._baudrate = baudrate
        self._read_timeout_s = read_timeout_s
        self._command_timeout_s = command_timeout_s
        self._heartbeat_interval_s = heartbeat_interval_s
        self._protocol_mode = self._normalize_protocol_mode(protocol_mode)
        self._command_lock = threading.RLock()
        self._connect_lock = threading.Lock()
        self._stop_event = threading.Event()
        self._reader_thread: threading.Thread | None = None
        self._heartbeat_thread: threading.Thread | None = None
        self._serial: Any = None
        self._responses: queue.Queue[Any] = queue.Queue()
        self._latest_telemetry: dict[str, int] = self._default_telemetry()
        self._events: deque[dict[str, int | str]] = deque(maxlen=event_cache_size)
        self._last_activity_monotonic = time.monotonic()
        self._log_store = log_store
        self._rx_buffer = bytearray()
        self._binary_peer_seen = False
        self._binary_tx_seq = 0
        self._runtime_snapshot: dict[str, Any] = {}
        self._limits_snapshot: dict[str, Any] = {}
        self._safety_snapshot: dict[str, Any] = {}
        self._calibration_snapshot: dict[str, Any] = {}
        self._version_snapshot: dict[str, Any] = {}

    @staticmethod
    def _default_telemetry() -> dict[str, int]:
        return {
            "ts_ms": 0,
            "timestamp_us": 0,
            "position_counts": 0,
            "pos_um": 0,
            "pos_set_um": 0,
            "vel_um_s": 0,
            "vel_set_um_s": 0,
            "iq_ref_mA": 0,
            "iq_meas_mA": 0,
            "id_ref_mA": 0,
            "id_meas_mA": 0,
            "vbus_mV": 0,
            "commission_stage": 0,
            "status_flags": 0,
            "error_flags": 0,
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
            protocol_mode=os.getenv("MIKROTOM_PROTOCOL_MODE", cls.PROTOCOL_MODE_AUTO),
        )

    @classmethod
    def _normalize_protocol_mode(cls, mode: str) -> str:
        normalized = mode.strip().lower()
        if normalized not in {
            cls.PROTOCOL_MODE_ASCII,
            cls.PROTOCOL_MODE_BINARY,
            cls.PROTOCOL_MODE_AUTO,
        }:
            raise ValueError(f"unsupported protocol mode: {mode}")
        return normalized

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
                chunk_size = 1
                in_waiting = getattr(self._serial, "in_waiting", 0)
                if isinstance(in_waiting, int) and in_waiting > 0:
                    chunk_size = in_waiting
                raw = self._serial.read(chunk_size)
            except (OSError, SerialException):
                self._mark_disconnected()
                return

            if not raw:
                continue

            self._rx_buffer.extend(raw)

            while True:
                message = self._extract_next_message()
                if message is None:
                    break

                kind, payload = message
                if kind == "ascii":
                    line = str(payload).strip()
                    if not line:
                        continue
                    if line.startswith("TEL,"):
                        self._log_protocol("rx_tel", line)
                        self._handle_telemetry(line)
                    elif line.startswith("RSP,"):
                        self._log_protocol("rx_rsp", line)
                        self._responses.put(line)
                elif kind == "binary":
                    frame = payload
                    self._handle_binary_frame(frame)

    @staticmethod
    def _crc16_ccitt_false(data: bytes) -> int:
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                if crc & 0x8000:
                    crc = ((crc << 1) ^ 0x1021) & 0xFFFF
                else:
                    crc = (crc << 1) & 0xFFFF
        return crc

    def _extract_next_message(self) -> tuple[str, Any] | None:
        if not self._rx_buffer:
            return None

        if len(self._rx_buffer) >= 2 and self._rx_buffer[0] == self.SOF1 and self._rx_buffer[1] == self.SOF2:
            if len(self._rx_buffer) < 6:
                return None

            len_field = self._rx_buffer[2]
            total_len = 2 + 1 + len_field + 2
            if len_field < 3 or total_len > 2 + 1 + 1 + 2 + 64 + 2:
                del self._rx_buffer[0]
                return None
            if len(self._rx_buffer) < total_len:
                return None

            frame = bytes(self._rx_buffer[:total_len])
            del self._rx_buffer[:total_len]
            return ("binary", frame)

        newline_index = self._rx_buffer.find(b"\n")
        if newline_index == -1:
            newline_index = self._rx_buffer.find(b"\r")
        if newline_index == -1:
            if self._rx_buffer and self._rx_buffer[0] == self.SOF1:
                del self._rx_buffer[0]
            return None

        line = bytes(self._rx_buffer[:newline_index])
        del self._rx_buffer[: newline_index + 1]
        return ("ascii", line.decode("utf-8", errors="replace"))

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
                with self._command_lock:
                    if self._binary_transport_active():
                        self._send_binary_heartbeat()
                    else:
                        self._send_ascii_command_line("CMD HEARTBEAT")
            except FirmwareError:
                continue

    @staticmethod
    def _u16_le(data: bytes, offset: int) -> int:
        return int.from_bytes(data[offset : offset + 2], "little", signed=False)

    @staticmethod
    def _u32_le(data: bytes, offset: int) -> int:
        return int.from_bytes(data[offset : offset + 4], "little", signed=False)

    @staticmethod
    def _i16_le(data: bytes, offset: int) -> int:
        return int.from_bytes(data[offset : offset + 2], "little", signed=True)

    @staticmethod
    def _i32_le(data: bytes, offset: int) -> int:
        return int.from_bytes(data[offset : offset + 4], "little", signed=True)

    def _handle_binary_frame(self, frame: bytes) -> None:
        if len(frame) < 8:
            return

        len_field = frame[2]
        frame_type = frame[3]
        seq = self._u16_le(frame, 4)
        payload = frame[6:-2]
        crc_rx = self._u16_le(frame, len(frame) - 2)
        crc_calc = self._crc16_ccitt_false(frame[2:-2])
        crc_ok = crc_rx == crc_calc
        hex_line = frame.hex(" ").upper()
        self._log_protocol("rx_bin", hex_line)

        if not crc_ok:
            self._responses.put({"kind": "BIN_ERROR", "seq": seq, "error": "CRC_FAIL"})
            return

        if len_field != (1 + 2 + len(payload)):
            self._responses.put({"kind": "BIN_ERROR", "seq": seq, "error": "LEN_FAIL"})
            return

        self._binary_peer_seen = True

        if frame_type == self.FRAME_TYPE_TELEMETRY:
            self._handle_binary_telemetry(seq, payload, crc_ok, hex_line)
        elif frame_type == self.FRAME_TYPE_RESPONSE:
            self._responses.put(self._parse_binary_response(seq, payload))
        elif frame_type == self.FRAME_TYPE_ERROR:
            self._responses.put(self._parse_binary_error(seq, payload))
        elif frame_type == self.FRAME_TYPE_HEARTBEAT:
            self._last_activity_monotonic = time.monotonic()
            self._responses.put(
                {
                    "kind": "BIN_HEARTBEAT",
                    "seq": seq,
                    "role": payload[0] if payload else 0,
                    "payload": payload,
                }
            )

    def _handle_binary_telemetry(self, seq: int, payload: bytes, crc_ok: bool, raw_hex: str) -> None:
        if not payload:
            return

        stream_id = payload[0]
        if stream_id == self.STREAM_ID_FAST_MOTION and len(payload) >= 41:
            telemetry = {
                "ts_ms": self._u32_le(payload, 1) // 1000,
                "timestamp_us": self._u32_le(payload, 1),
                "position_counts": self._i32_le(payload, 5),
                "pos_um": self._i32_le(payload, 9),
                "pos_set_um": self._i32_le(payload, 13),
                "vel_um_s": self._i32_le(payload, 17),
                "vel_set_um_s": self._i32_le(payload, 21),
                "iq_ref_mA": self._i16_le(payload, 25),
                "iq_meas_mA": self._i16_le(payload, 27),
                "id_ref_mA": self._i16_le(payload, 29),
                "id_meas_mA": self._i16_le(payload, 31),
                "vbus_mV": self._u16_le(payload, 33),
                "state": payload[35],
                "commission_stage": payload[36],
                "status_flags": self._u16_le(payload, 37),
                "error_flags": self._u16_le(payload, 39),
                "fault": self._latest_telemetry.get("fault", 0),
                "seq": seq,
            }
            self._latest_telemetry = telemetry
            if self._log_store is not None:
                self._log_store.log_telemetry(self._port, telemetry)
        elif stream_id == self.STREAM_ID_RUNTIME_STATUS and len(payload) >= 18:
            snapshot = self._decode_binary_snapshot_body(seq, payload)
            snapshot["kind"] = "BIN_STATUS"
            self._cache_snapshot(snapshot)
            self._responses.put(snapshot)

    def _parse_binary_response(self, seq: int, payload: bytes) -> dict[str, Any]:
        if len(payload) < 5:
            return {"kind": "BIN_RESPONSE", "seq": seq, "malformed": True}

        response_id = payload[0]
        ack_seq = self._u16_le(payload, 1)
        status_code = self._u16_le(payload, 3)
        body = payload[5:]

        if response_id == 0x01:
            return {"kind": "BIN_ACK", "seq": seq, "ack_seq": ack_seq, "status_code": status_code}
        if response_id == 0x02:
            return {"kind": "BIN_NACK", "seq": seq, "ack_seq": ack_seq, "status_code": status_code}
        if response_id == 0x03:
            snapshot = self._decode_binary_snapshot_body(seq, body)
            return {
                "kind": "BIN_SNAPSHOT",
                "seq": seq,
                "ack_seq": ack_seq,
                "status_code": status_code,
                "body": body,
                "snapshot": snapshot,
            }
        return {
            "kind": "BIN_RESPONSE",
            "seq": seq,
            "ack_seq": ack_seq,
            "status_code": status_code,
            "body": body,
        }

    def _parse_binary_error(self, seq: int, payload: bytes) -> dict[str, Any]:
        if len(payload) < 10:
            return {"kind": "BIN_ERROR", "seq": seq, "malformed": True}

        return {
            "kind": "BIN_ERROR",
            "seq": seq,
            "error_code": payload[0],
            "ref_seq": self._u16_le(payload, 1),
            "detail_code": self._u16_le(payload, 3),
            "fault_mask": self._u32_le(payload, 5),
            "axis_state": payload[9],
        }

    def _decode_binary_snapshot_body(self, seq: int, payload: bytes) -> dict[str, Any]:
        if not payload:
            return {"kind": "snapshot", "snapshot_kind": "unknown", "stream_id": 0, "seq": seq}

        stream_id = payload[0]

        if stream_id == self.STREAM_ID_RUNTIME_STATUS and len(payload) >= 18:
            snapshot: dict[str, Any] = {
                "kind": "snapshot",
                "snapshot_kind": "runtime_status",
                "stream_id": stream_id,
                "seq": seq,
                "timestamp_us": self._u32_le(payload, 1),
                "fault_code": payload[5],
                "fault_mask": self._u32_le(payload, 6),
                "axis_state": payload[10],
                "commission_stage": payload[11],
                "status_flags": self._u16_le(payload, 12),
                "error_flags": self._u16_le(payload, 14),
                "vbus_mV": self._u16_le(payload, 16),
            }
            if len(payload) >= 28:
                snapshot.update(
                    {
                        "override_flags": self._u16_le(payload, 14),
                        "event_count": self._u16_le(payload, 16),
                        "first_move_max_delta_um": self._i16_le(payload, 18),
                        "telemetry_period_ms": self._u16_le(payload, 20),
                        "vbus_min_mV": self._u16_le(payload, 22),
                        "vbus_max_mV": self._u16_le(payload, 24),
                    }
                )
            self._runtime_snapshot = snapshot
            return snapshot

        if stream_id == self.STREAM_ID_LIMITS and len(payload) >= 25:
            snapshot = {
                "kind": "snapshot",
                "snapshot_kind": "limits",
                "stream_id": stream_id,
                "seq": seq,
                "timestamp_us": self._u32_le(payload, 1),
                "max_current_nominal_mA": self._u16_le(payload, 5),
                "max_current_peak_mA": self._u16_le(payload, 7),
                "max_velocity_um_s": self._u32_le(payload, 9),
                "max_accel_um_s2": self._u32_le(payload, 13),
                "soft_min_pos_um": self._i32_le(payload, 17),
                "soft_max_pos_um": self._i32_le(payload, 21),
            }
            self._limits_snapshot = snapshot
            return snapshot

        if stream_id == self.STREAM_ID_SAFETY_CONFIG and len(payload) >= 13:
            snapshot = {
                "kind": "snapshot",
                "snapshot_kind": "safety_config",
                "stream_id": stream_id,
                "seq": seq,
                "timestamp_us": self._u32_le(payload, 1),
                "install_flags": self._u16_le(payload, 5),
                "override_flags": self._u16_le(payload, 7),
                "behavior_flags": self._u16_le(payload, 9),
                "reserved": self._u16_le(payload, 11),
            }
            self._safety_snapshot = snapshot
            return snapshot

        if stream_id == self.STREAM_ID_CALIBRATION and len(payload) >= 25:
            pitch_um_x1000 = self._u32_le(payload, 13)
            snapshot = {
                "kind": "snapshot",
                "snapshot_kind": "calibration",
                "stream_id": stream_id,
                "seq": seq,
                "timestamp_us": self._u32_le(payload, 1),
                "calib_valid": payload[5],
                "calib_sign": int.from_bytes(payload[6:7], "little", signed=True),
                "zero_pos_um": self._i32_le(payload, 9),
                "pitch_um_x1000": pitch_um_x1000,
                "pitch_um": pitch_um_x1000 / 1000.0,
                "endstop_left_um": self._i32_le(payload, 17),
                "endstop_right_um": self._i32_le(payload, 21),
            }
            self._calibration_snapshot = snapshot
            return snapshot

        if stream_id == self.STREAM_ID_VERSION and len(payload) >= 19:
            snapshot = {
                "kind": "snapshot",
                "snapshot_kind": "version",
                "stream_id": stream_id,
                "seq": seq,
                "protocol_version_major": payload[1],
                "protocol_version_minor": payload[2],
                "fw_version_major": payload[3],
                "fw_version_minor": payload[4],
                "fw_version_patch": self._u16_le(payload, 5),
                "build_unix_time": self._u32_le(payload, 7),
                "git_hash32": self._u32_le(payload, 11),
                "capability_flags": self._u32_le(payload, 15),
            }
            build_unix_time = int(snapshot["build_unix_time"])
            snapshot["build_time_utc"] = (
                datetime.fromtimestamp(build_unix_time, tz=timezone.utc).isoformat()
                if build_unix_time > 0
                else ""
            )
            self._version_snapshot = snapshot
            return snapshot

        return {
            "kind": "snapshot",
            "snapshot_kind": "unknown",
            "stream_id": stream_id,
            "seq": seq,
            "payload_len": len(payload),
            "payload_hex": payload.hex(" ").upper(),
        }

    def _cache_snapshot(self, snapshot: dict[str, Any]) -> None:
        if not snapshot or self._log_store is None:
            return
        self._log_store.log_snapshot(
            self._port,
            int(snapshot.get("seq", 0)),
            int(snapshot.get("stream_id", 0)),
            str(snapshot.get("snapshot_kind", "unknown")),
            snapshot,
        )

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

    def _binary_transport_active(self) -> bool:
        return self._protocol_mode == self.PROTOCOL_MODE_BINARY or (
            self._protocol_mode == self.PROTOCOL_MODE_AUTO and self._binary_peer_seen
        )

    def _send_binary_frame(self, frame_type: int, payload: bytes) -> int:
        self._ensure_connected()

        if self._serial is None:
            raise FirmwareError("serial transport is not connected")

        if len(payload) > 64:
            raise FirmwareError("binary payload too large")

        seq = self._binary_tx_seq & 0xFFFF
        self._binary_tx_seq = (self._binary_tx_seq + 1) & 0xFFFF
        len_field = 1 + 2 + len(payload)
        body = bytes([len_field, frame_type]) + seq.to_bytes(2, "little") + payload
        crc = self._crc16_ccitt_false(body).to_bytes(2, "little")
        frame = bytes([self.SOF1, self.SOF2]) + body + crc

        try:
            self._serial.write(frame)
            self._serial.flush()
        except (OSError, SerialException) as exc:
            self._mark_disconnected()
            raise FirmwareError(f"serial write failed: {exc}") from exc

        self._log_protocol("tx_bin", frame.hex(" ").upper())
        return seq

    def _binary_error_name(self, code: int) -> str:
        names = {
            self.ERR_CRC_FAIL: "CRC_FAIL",
            self.ERR_LEN_FAIL: "LEN_FAIL",
            self.ERR_UNKNOWN_TYPE: "UNKNOWN_TYPE",
            self.ERR_UNKNOWN_COMMAND: "UNKNOWN_COMMAND",
            self.ERR_STATE_REJECTED: "STATE_REJECTED",
            self.ERR_BUSY: "BUSY",
        }
        return names.get(code, f"ERR_{code}")

    def _binary_response_matches(self, response: dict[str, Any], seq: int) -> bool:
        kind = response.get("kind")
        if kind in {"BIN_ACK", "BIN_NACK", "BIN_SNAPSHOT"}:
            return int(response.get("ack_seq", -1)) == seq
        if kind == "BIN_ERROR":
            return int(response.get("ref_seq", response.get("seq", -1))) == seq
        return False

    def _build_binary_motion_payload(
        self,
        command_id: int,
        target_um: int,
        velocity_um_s: int = 0,
        accel_um_s2: int = 0,
        iq_limit_mA: int = 0,
    ) -> bytes:
        payload = bytearray(18)
        payload[0] = command_id & 0xFF
        payload[4:8] = int(target_um).to_bytes(4, "little", signed=True)
        payload[8:12] = int(velocity_um_s).to_bytes(4, "little", signed=True)
        payload[12:16] = int(accel_um_s2).to_bytes(4, "little", signed=True)
        payload[16:18] = int(iq_limit_mA).to_bytes(2, "little", signed=True)
        return bytes(payload)

    def _build_binary_command_request(self, line: str) -> tuple[int, bytes] | None:
        tokens = line.strip().split()
        if len(tokens) < 2 or tokens[0].upper() != "CMD":
            return None

        command = tokens[1].upper()
        simple_commands = {
            "ENABLE": self.CMD_ENABLE,
            "DISABLE": self.CMD_DISABLE,
            "STOP": self.CMD_STOP,
            "ACK_FAULT": self.CMD_RESET_FAULT,
            "CALIB_ZERO": self.CMD_CALIBRATION_ZERO,
            "REQUEST_STATUS": self.CMD_REQUEST_STATUS,
        }

        if command in simple_commands:
            payload = bytearray([simple_commands[command], 0x00, 0x00, 0x00])
            if command == "REQUEST_STATUS":
                request_mask = self.REQUEST_RUNTIME_STATUS
                if len(tokens) >= 3:
                    request_mask = int(tokens[2], 0)
                payload.extend(int(request_mask).to_bytes(2, "little", signed=False))
            return (self.FRAME_TYPE_COMMAND, payload)

        if command == "MOVE_REL" and len(tokens) >= 3:
            target_um = int(tokens[2], 10)
            return (
                self.FRAME_TYPE_COMMAND,
                self._build_binary_motion_payload(self.CMD_MOVE_RELATIVE, target_um),
            )

        if command == "MOVE_ABS" and len(tokens) >= 3:
            target_um = int(tokens[2], 10)
            return (
                self.FRAME_TYPE_COMMAND,
                self._build_binary_motion_payload(self.CMD_MOVE_ABSOLUTE, target_um),
            )

        return None

    def _send_ascii_command_line(self, line: str, expected=None) -> dict[str, Any]:
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
            if isinstance(response, str):
                parsed = self._parse_response(response)
            else:
                parsed = response
            if expected is None or expected(parsed):
                return parsed

        raise FirmwareError(f"timeout waiting for response to: {line}")

    def _send_binary_request_frame(self, frame_type: int, payload: bytes, label: str, expected=None) -> dict[str, Any]:
        self._last_activity_monotonic = time.monotonic()
        self._clear_pending_responses()
        seq = self._send_binary_frame(frame_type, payload)

        deadline = time.monotonic() + self._command_timeout_s
        while time.monotonic() < deadline:
            timeout = max(0.01, deadline - time.monotonic())
            try:
                response = self._responses.get(timeout=timeout)
            except queue.Empty:
                continue
            self._last_activity_monotonic = time.monotonic()
            if isinstance(response, str):
                parsed = self._parse_response(response)
            else:
                parsed = response

            if not isinstance(parsed, dict):
                continue
            if not self._binary_response_matches(parsed, seq):
                continue

            kind = parsed.get("kind")
            if kind == "BIN_NACK":
                raise FirmwareError(self._binary_error_name(int(parsed.get("status_code", 0))))
            if kind == "BIN_ERROR":
                error_code = int(parsed.get("error_code", 0))
                raise FirmwareError(self._binary_error_name(error_code))
            if kind == "BIN_SNAPSHOT":
                snapshot = parsed.get("snapshot")
                if isinstance(snapshot, dict):
                    self._cache_snapshot(snapshot)
            if expected is None or expected(parsed):
                return parsed

        raise FirmwareError(f"timeout waiting for binary response to: {label}")

    def _send_binary_command_line(self, line: str, expected=None) -> dict[str, Any]:
        request = self._build_binary_command_request(line)
        if request is None:
            raise FirmwareError(f"unsupported binary command: {line}")
        frame_type, payload = request
        return self._send_binary_request_frame(frame_type, payload, line, expected=expected)

    def _send_binary_heartbeat(self) -> None:
        self._last_activity_monotonic = time.monotonic()
        self._send_binary_frame(self.FRAME_TYPE_HEARTBEAT, bytes([self.HEARTBEAT_PC_TO_MCU]))

    @staticmethod
    def _snapshot_count_from_mask(request_mask: int) -> int:
        mask = request_mask & 0xFFFF
        count = 0
        while mask:
            count += mask & 1
            mask >>= 1
        return count or 1

    def request_binary_snapshots(self, request_mask: int) -> list[dict[str, Any]]:
        if not self._binary_transport_active():
            raise FirmwareError("binary snapshot request requires active binary transport")

        payload = bytes(
            [
                self.CMD_REQUEST_STATUS,
                0x00,
                0x00,
                0x00,
            ]
        ) + int(request_mask).to_bytes(2, "little", signed=False)

        with self._command_lock:
            self._last_activity_monotonic = time.monotonic()
            self._clear_pending_responses()
            seq = self._send_binary_frame(self.FRAME_TYPE_COMMAND, payload)
            expected_count = self._snapshot_count_from_mask(request_mask)
            collected: list[dict[str, Any]] = []
            deadline = time.monotonic() + self._command_timeout_s

            while time.monotonic() < deadline:
                timeout = max(0.01, deadline - time.monotonic())
                try:
                    response = self._responses.get(timeout=timeout)
                except queue.Empty:
                    continue

                self._last_activity_monotonic = time.monotonic()
                if isinstance(response, str):
                    continue
                if not isinstance(response, dict):
                    continue
                if not self._binary_response_matches(response, seq):
                    continue

                kind = response.get("kind")
                if kind == "BIN_NACK":
                    raise FirmwareError(self._binary_error_name(int(response.get("status_code", 0))))
                if kind == "BIN_ERROR":
                    raise FirmwareError(self._binary_error_name(int(response.get("error_code", 0))))
                if kind != "BIN_SNAPSHOT":
                    continue

                snapshot = response.get("snapshot")
                if isinstance(snapshot, dict):
                    self._cache_snapshot(snapshot)
                    collected.append(snapshot)
                    if len(collected) >= expected_count:
                        return collected

            raise FirmwareError(
                f"timeout waiting for binary snapshots: expected {expected_count}, got {len(collected)}"
            )

    def _send_command(self, line: str, expected=None) -> dict[str, Any]:
        with self._command_lock:
            request = self._build_binary_command_request(line)
            if request is not None and self._binary_transport_active():
                return self._send_binary_command_line(line, expected=expected)
            return self._send_ascii_command_line(line, expected=expected)

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
        response = self._send_command(
            f"GET {name}",
            expected=lambda rsp: (
                (name == "STATE" and rsp.get("kind") == "STATE") or
                (name == "FAULT" and rsp.get("kind") == "FAULT") or
                (rsp.get("kind") in {"PARAM", "CONFIG"} and rsp.get("name") == name)
            ),
        )
        return str(response["value"])

    def _read_param_value_default(self, name: str, default: str) -> str:
        try:
            return self._read_param_value(name)
        except FirmwareError:
            return default

    def _read_param(self, name: str) -> str:
        response = self._send_command(
            f"GET PARAM {name}",
            expected=lambda rsp: rsp.get("kind") == "PARAM" and rsp.get("name") == name,
        )
        return str(response["value"])

    def _read_config(self, name: str) -> str:
        response = self._send_command(
            f"GET CONFIG {name}",
            expected=lambda rsp: rsp.get("kind") == "CONFIG" and rsp.get("name") == name,
        )
        return str(response["value"])

    def _log_protocol(self, direction: str, line: str) -> None:
        if self._log_store is not None:
            self._log_store.log_protocol(self._port, direction, line)

    def latest_telemetry(self) -> dict[str, int]:
        self._ensure_connected()
        return dict(self._latest_telemetry)

    def drain_events(self, max_events: int = 64) -> list[dict[str, int | str]]:
        with self._command_lock:
            drained: list[dict[str, int | str]] = []

            try:
                count = int(self._read_param_value("EVENT_COUNT"))
            except (ValueError, FirmwareError):
                return drained

            for _ in range(min(count, max_events)):
                event = self._send_command(
                    "GET EVENT_POP",
                    expected=lambda rsp: rsp.get("kind") == "EVENT",
                )
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
        with self._command_lock:
            version = self._send_command(
                "GET VERSION",
                expected=lambda rsp: rsp.get("kind") == "VERSION",
            )
            return {
                "name": str(version.get("name", "")),
                "version": str(version.get("version", "")),
                "build_date": str(version.get("build_date", "")),
                "git_hash": str(version.get("git_hash", "")),
            }

    def status(self) -> dict[str, Any]:
        with self._command_lock:
            telemetry = self.latest_telemetry()

            return {
                "axis_state": self._send_command(
                    "GET STATE",
                    expected=lambda rsp: rsp.get("kind") == "STATE",
                )["value"],
                "fault": self._send_command(
                    "GET FAULT",
                    expected=lambda rsp: rsp.get("kind") == "FAULT",
                )["value"],
                "fault_mask": int(self._read_param_value("FAULT_MASK")),
                "commissioning_stage": int(self._read_param_value("COMMISSION_STAGE")),
                "safe_mode": int(self._read_param_value("SAFE_MODE")),
                "arming_only": int(self._read_param_value("ARMING_ONLY")),
                "controlled_motion": int(self._read_param_value("CONTROLLED_MOTION")),
                "run_allowed": int(self._read_param_value("RUN_ALLOWED")),
                "enabled": int(self._read_param_value("ENABLED")),
                "homing_started": int(self._read_param_value("HOMING_STARTED")),
                "homing_successful": int(self._read_param_value("HOMING_SUCCESSFUL")),
                "homing_ongoing": int(self._read_param_value("HOMING_ONGOING")),
                "first_move_test_active": int(self._read_param_value_default("FIRST_MOVE_TEST_ACTIVE", "0")),
                "first_move_max_delta_um": int(self._read_param_value_default("FIRST_MOVE_MAX_DELTA", "100")),
                "telemetry_enabled": int(self._read_param_value_default("TELEMETRY_ENABLED", "1")),
                "config_loaded": int(self._read_param_value("CONFIG_LOADED")),
                "safe_integration": int(self._read_param_value("SAFE_INTEGRATION")),
                "motion_implemented": int(self._read_param_value("MOTION_IMPLEMENTED")),
                "position_um": int(self._read_param_value("POS")),
                "vbus_V": float(self._read_param_value("VBUS")),
                "vbus_valid": int(self._read_param_value("VBUS_VALID")),
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
        with self._command_lock:
            status = self.status()
            telemetry = self.latest_telemetry()
            events = self.recent_events(limit=16)

            return {
                "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                "transport": {
                    "port": self._port,
                    "baudrate": self._baudrate,
                    "protocol_mode": self._protocol_mode,
                    "binary_peer_seen": self._binary_peer_seen,
                    "binary_transport_active": self._binary_transport_active(),
                    "read_timeout_s": self._read_timeout_s,
                    "command_timeout_s": self._command_timeout_s,
                    "heartbeat_interval_s": self._heartbeat_interval_s,
                    "connected": self._serial is not None,
                    "seconds_since_activity": round(time.monotonic() - self._last_activity_monotonic, 3),
                    "event_cache_depth": len(self._events),
                },
                "firmware_version": self.version_info(),
                "binary_snapshots": {
                    "runtime_status": dict(self._runtime_snapshot),
                    "limits": dict(self._limits_snapshot),
                    "safety_config": dict(self._safety_snapshot),
                    "calibration": dict(self._calibration_snapshot),
                    "version": dict(self._version_snapshot),
                },
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
        with self._command_lock:
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
            "telemetry_enabled": lambda value: f"SET CONFIG TELEMETRY_ENABLED {int(value)}",
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

        with self._command_lock:
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
