from __future__ import annotations

import pytest

from agent.bridge import FirmwareError, FirmwareTransport


class FakeLogStore:
    def __init__(self) -> None:
        self.telemetry_rows: list[tuple[str, dict[str, int]]] = []
        self.event_rows: list[tuple[str, dict[str, int | str]]] = []

    def log_telemetry(self, port: str, sample: dict[str, int]) -> None:
        self.telemetry_rows.append((port, sample))

    def log_event(self, port: str, event: dict[str, int | str]) -> None:
        self.event_rows.append((port, event))


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
    monkeypatch.setattr(transport, "_send_command", lambda line: next(responses))

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
