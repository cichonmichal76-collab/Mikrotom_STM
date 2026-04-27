from __future__ import annotations

import pytest

from agent.bridge import FirmwareError, FirmwareTransport


class FakeLogStore:
    def __init__(self) -> None:
        self.protocol_rows: list[tuple[str, str, str]] = []
        self.telemetry_rows: list[tuple[str, dict[str, int]]] = []
        self.event_rows: list[tuple[str, dict[str, int | str]]] = []
        self.snapshot_rows: list[tuple[str, int, int, str, dict[str, object]]] = []

    def log_protocol(self, port: str, direction: str, line: str) -> None:
        self.protocol_rows.append((port, direction, line))

    def log_telemetry(self, port: str, sample: dict[str, int]) -> None:
        self.telemetry_rows.append((port, sample))

    def log_event(self, port: str, event: dict[str, int | str]) -> None:
        self.event_rows.append((port, event))

    def log_snapshot(
        self,
        port: str,
        seq: int,
        stream_id: int,
        snapshot_kind: str,
        payload: dict[str, object],
    ) -> None:
        self.snapshot_rows.append((port, seq, stream_id, snapshot_kind, payload))


@pytest.mark.parametrize(
    ("line", "expected"),
    [
        ("RSP,OK", {"kind": "OK"}),
        ("RSP,STATE,READY", {"kind": "STATE", "value": "READY"}),
        ("RSP,FAULT,NONE", {"kind": "FAULT", "value": "NONE"}),
        (
            "RSP,VERSION,Mikrotom,1.0.0,2026-04-23,abc123",
            {
                "kind": "VERSION",
                "name": "Mikrotom",
                "version": "1.0.0",
                "build_date": "2026-04-23",
                "git_hash": "abc123",
            },
        ),
        (
            "RSP,EVENT,123,BOOT,0",
            {"kind": "EVENT", "empty": False, "ts_ms": 123, "code": "BOOT", "value": 0},
        ),
        ("RSP,EVENT,NONE", {"kind": "EVENT", "empty": True}),
    ],
)
def test_parse_response_supported_variants(line, expected):
    assert FirmwareTransport._parse_response(line) == expected


def test_parse_response_raises_for_error_line():
    with pytest.raises(FirmwareError, match="BAD_THING"):
        FirmwareTransport._parse_response("RSP,ERR,BAD_THING")


def test_handle_telemetry_updates_latest_sample_and_logs():
    log_store = FakeLogStore()
    transport = FirmwareTransport(port="COM_TEST", log_store=log_store)

    transport._handle_telemetry("TEL,1,2,3,4,5,6,7,8,9")

    assert transport._latest_telemetry == {
        "ts_ms": 1,
        "pos_um": 2,
        "pos_set_um": 3,
        "vel_um_s": 4,
        "vel_set_um_s": 5,
        "iq_ref_mA": 6,
        "iq_meas_mA": 7,
        "state": 8,
        "fault": 9,
    }
    assert log_store.telemetry_rows == [
        (
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
    ]


def _build_v2_frame(frame_type: int, seq: int, payload: bytes) -> bytes:
    transport = FirmwareTransport(port="COM_TEST")
    len_field = 1 + 2 + len(payload)
    body = bytes([len_field, frame_type]) + seq.to_bytes(2, "little") + payload
    crc = transport._crc16_ccitt_false(body).to_bytes(2, "little")
    return bytes([transport.SOF1, transport.SOF2]) + body + crc


def test_extracts_ascii_and_binary_messages():
    transport = FirmwareTransport(port="COM_TEST")
    transport._rx_buffer.extend(b"RSP,OK\n")
    assert transport._extract_next_message() == ("ascii", "RSP,OK")

    payload = (
        bytes([transport.STREAM_ID_FAST_MOTION])
        + (123000).to_bytes(4, "little")
        + (10).to_bytes(4, "little", signed=True)
        + (20).to_bytes(4, "little", signed=True)
        + (30).to_bytes(4, "little", signed=True)
        + (40).to_bytes(4, "little", signed=True)
        + (50).to_bytes(4, "little", signed=True)
        + (60).to_bytes(2, "little", signed=True)
        + (70).to_bytes(2, "little", signed=True)
        + (80).to_bytes(2, "little", signed=True)
        + (90).to_bytes(2, "little", signed=True)
        + (12000).to_bytes(2, "little")
        + bytes([6, 3])
        + (0x1234).to_bytes(2, "little")
        + (0x0002).to_bytes(2, "little")
    )
    frame = _build_v2_frame(transport.FRAME_TYPE_TELEMETRY, 7, payload)
    transport._rx_buffer.extend(frame)
    kind, raw_frame = transport._extract_next_message()
    assert kind == "binary"
    assert raw_frame == frame


def test_handle_binary_fast_motion_updates_latest_sample_and_logs():
    log_store = FakeLogStore()
    transport = FirmwareTransport(port="COM_TEST", log_store=log_store)

    payload = (
        bytes([transport.STREAM_ID_FAST_MOTION])
        + (123000).to_bytes(4, "little")
        + (10).to_bytes(4, "little", signed=True)
        + (20).to_bytes(4, "little", signed=True)
        + (30).to_bytes(4, "little", signed=True)
        + (40).to_bytes(4, "little", signed=True)
        + (50).to_bytes(4, "little", signed=True)
        + (60).to_bytes(2, "little", signed=True)
        + (70).to_bytes(2, "little", signed=True)
        + (80).to_bytes(2, "little", signed=True)
        + (90).to_bytes(2, "little", signed=True)
        + (12000).to_bytes(2, "little")
        + bytes([6, 3])
        + (0x1234).to_bytes(2, "little")
        + (0x0002).to_bytes(2, "little")
    )
    frame = _build_v2_frame(transport.FRAME_TYPE_TELEMETRY, 7, payload)

    transport._handle_binary_frame(frame)

    assert transport._latest_telemetry["timestamp_us"] == 123000
    assert transport._latest_telemetry["ts_ms"] == 123
    assert transport._latest_telemetry["position_counts"] == 10
    assert transport._latest_telemetry["pos_um"] == 20
    assert transport._latest_telemetry["iq_ref_mA"] == 60
    assert transport._latest_telemetry["id_meas_mA"] == 90
    assert transport._latest_telemetry["vbus_mV"] == 12000
    assert transport._latest_telemetry["state"] == 6
    assert transport._latest_telemetry["commission_stage"] == 3
    assert transport._latest_telemetry["status_flags"] == 0x1234
    assert transport._latest_telemetry["error_flags"] == 0x0002
    assert log_store.telemetry_rows
    assert any(direction == "rx_bin" for _, direction, _ in log_store.protocol_rows)


def test_build_binary_command_request_for_move_rel():
    transport = FirmwareTransport(port="COM_TEST", protocol_mode="binary")

    frame_type, payload = transport._build_binary_command_request("CMD MOVE_REL 250")

    assert frame_type == transport.FRAME_TYPE_COMMAND
    assert len(payload) == 18
    assert payload[0] == transport.CMD_MOVE_RELATIVE
    assert int.from_bytes(payload[4:8], "little", signed=True) == 250
    assert int.from_bytes(payload[8:12], "little", signed=True) == 0
    assert int.from_bytes(payload[12:16], "little", signed=True) == 0
    assert int.from_bytes(payload[16:18], "little", signed=True) == 0


def test_send_command_uses_binary_for_supported_command_in_binary_mode(monkeypatch: pytest.MonkeyPatch):
    transport = FirmwareTransport(port="COM_TEST", protocol_mode="binary")
    calls: list[tuple[str, str]] = []

    def fake_binary(line: str, expected=None):
        calls.append(("binary", line))
        return {"kind": "BIN_ACK", "ack_seq": 1, "status_code": 0}

    def fake_ascii(line: str, expected=None):
        calls.append(("ascii", line))
        return {"kind": "OK"}

    monkeypatch.setattr(transport, "_send_binary_command_line", fake_binary)
    monkeypatch.setattr(transport, "_send_ascii_command_line", fake_ascii)

    response = transport._send_command("CMD ENABLE")

    assert response["kind"] == "BIN_ACK"
    assert calls == [("binary", "CMD ENABLE")]


def test_send_command_falls_back_to_ascii_for_unsupported_binary_command(monkeypatch: pytest.MonkeyPatch):
    transport = FirmwareTransport(port="COM_TEST", protocol_mode="binary")
    calls: list[tuple[str, str]] = []

    def fake_binary(line: str, expected=None):
        calls.append(("binary", line))
        return {"kind": "BIN_ACK", "ack_seq": 1, "status_code": 0}

    def fake_ascii(line: str, expected=None):
        calls.append(("ascii", line))
        return {"kind": "OK"}

    monkeypatch.setattr(transport, "_send_binary_command_line", fake_binary)
    monkeypatch.setattr(transport, "_send_ascii_command_line", fake_ascii)

    response = transport._send_command("CMD HOME")

    assert response["kind"] == "OK"
    assert calls == [("ascii", "CMD HOME")]


def test_send_command_stays_ascii_in_auto_mode_until_binary_peer_is_seen(monkeypatch: pytest.MonkeyPatch):
    transport = FirmwareTransport(port="COM_TEST", protocol_mode="auto")
    calls: list[tuple[str, str]] = []

    def fake_binary(line: str, expected=None):
        calls.append(("binary", line))
        return {"kind": "BIN_ACK", "ack_seq": 1, "status_code": 0}

    def fake_ascii(line: str, expected=None):
        calls.append(("ascii", line))
        return {"kind": "OK"}

    monkeypatch.setattr(transport, "_send_binary_command_line", fake_binary)
    monkeypatch.setattr(transport, "_send_ascii_command_line", fake_ascii)

    response = transport._send_command("CMD ENABLE")

    assert response["kind"] == "OK"
    assert calls == [("ascii", "CMD ENABLE")]

    transport._binary_peer_seen = True
    transport._send_command("CMD ENABLE")
    assert calls[-1] == ("binary", "CMD ENABLE")


def test_parse_binary_snapshot_response_for_limits():
    transport = FirmwareTransport(port="COM_TEST")
    body = (
        bytes([transport.STREAM_ID_LIMITS])
        + (111000).to_bytes(4, "little")
        + (200).to_bytes(2, "little")
        + (3000).to_bytes(2, "little")
        + (5000).to_bytes(4, "little")
        + (7000).to_bytes(4, "little")
        + (-100).to_bytes(4, "little", signed=True)
        + (200).to_bytes(4, "little", signed=True)
    )
    payload = bytes([transport.RSP_SNAPSHOT]) + (12).to_bytes(2, "little") + (0).to_bytes(2, "little") + body

    response = transport._parse_binary_response(33, payload)

    assert response["kind"] == "BIN_SNAPSHOT"
    snapshot = response["snapshot"]
    assert snapshot["snapshot_kind"] == "limits"
    assert snapshot["max_current_peak_mA"] == 3000
    assert snapshot["soft_min_pos_um"] == -100
    assert snapshot["soft_max_pos_um"] == 200


def test_request_binary_snapshots_collects_all_requested(monkeypatch: pytest.MonkeyPatch):
    log_store = FakeLogStore()
    transport = FirmwareTransport(port="COM_TEST", protocol_mode="binary", log_store=log_store)
    transport._binary_peer_seen = True

    def fake_send_binary_frame(frame_type: int, payload: bytes) -> int:
        assert frame_type == transport.FRAME_TYPE_COMMAND
        assert payload[0] == transport.CMD_REQUEST_STATUS
        assert int.from_bytes(payload[4:6], "little") == (
            transport.REQUEST_RUNTIME_STATUS | transport.REQUEST_LIMITS
        )
        runtime_snapshot = {
            "kind": "BIN_SNAPSHOT",
            "ack_seq": 77,
            "snapshot": {
                "snapshot_kind": "runtime_status",
                "stream_id": transport.STREAM_ID_RUNTIME_STATUS,
                "seq": 10,
            },
        }
        limits_snapshot = {
            "kind": "BIN_SNAPSHOT",
            "ack_seq": 77,
            "snapshot": {
                "snapshot_kind": "limits",
                "stream_id": transport.STREAM_ID_LIMITS,
                "seq": 11,
            },
        }
        transport._responses.put(runtime_snapshot)
        transport._responses.put(limits_snapshot)
        return 77

    monkeypatch.setattr(transport, "_send_binary_frame", fake_send_binary_frame)

    snapshots = transport.request_binary_snapshots(
        transport.REQUEST_RUNTIME_STATUS | transport.REQUEST_LIMITS
    )

    assert [item["snapshot_kind"] for item in snapshots] == ["runtime_status", "limits"]
    assert len(log_store.snapshot_rows) == 2


def test_apply_params_maps_payload_to_firmware_commands(monkeypatch: pytest.MonkeyPatch):
    issued: list[str] = []
    transport = FirmwareTransport(port="COM_TEST")

    def fake_send_command(line: str):
        issued.append(line)
        return {"kind": "OK"}

    monkeypatch.setattr(transport, "_send_command", fake_send_command)

    result = transport.apply_params(
        {
            "commissioning_stage": 3,
            "safe_mode": 0,
            "max_current": 0.3,
            "soft_min_pos": -100,
            "calib_sign": -1,
        }
    )

    assert result == {
        "ok": True,
        "commands": [
            "SET CONFIG COMMISSION_STAGE 3",
            "SET CONFIG SAFE_MODE 0",
            "SET PARAM MAX_CURRENT 0.300000",
            "SET PARAM SOFT_MIN_POS -100",
            "SET PARAM CALIB_SIGN -1",
        ],
    }
    assert issued == result["commands"]


def test_apply_params_rejects_read_only_calibration_flag():
    transport = FirmwareTransport(port="COM_TEST")

    with pytest.raises(FirmwareError, match="read-only"):
        transport.apply_params({"calib_valid": 1})


def test_drain_events_reads_firmware_queue_and_logs(monkeypatch: pytest.MonkeyPatch):
    log_store = FakeLogStore()
    transport = FirmwareTransport(port="COM_TEST", log_store=log_store)
    responses = iter(
        [
            {"kind": "EVENT", "empty": False, "ts_ms": 100, "code": "BOOT", "value": 0},
            {"kind": "EVENT", "empty": False, "ts_ms": 200, "code": "ENABLE", "value": 1},
        ]
    )

    monkeypatch.setattr(transport, "_read_param_value", lambda name: "2")
    monkeypatch.setattr(transport, "_send_command", lambda line, **kwargs: next(responses))

    drained = transport.drain_events()
    monkeypatch.setattr(transport, "_read_param_value", lambda name: "0")

    assert drained == [
        {"ts_ms": 100, "code": "BOOT", "value": 0},
        {"ts_ms": 200, "code": "ENABLE", "value": 1},
    ]
    assert transport.recent_events(limit=1) == [{"ts_ms": 200, "code": "ENABLE", "value": 1}]
    assert log_store.event_rows == [
        ("COM_TEST", {"ts_ms": 100, "code": "BOOT", "value": 0}),
        ("COM_TEST", {"ts_ms": 200, "code": "ENABLE", "value": 1}),
    ]
