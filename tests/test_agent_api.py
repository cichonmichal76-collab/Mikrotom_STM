from __future__ import annotations

import pytest

from agent.bridge import FirmwareError


class FakeTransport:
    def __init__(self) -> None:
        self.commands: list[str] = []
        self.params_payloads: list[dict[str, object]] = []

    def status(self):
        return {
            "axis_state": "CONFIG",
            "fault": "NONE",
            "run_allowed": 0,
            "enabled": 0,
        }

    def latest_telemetry(self):
        return {
            "ts_ms": 123,
            "pos_um": 10,
            "state": 1,
        }

    def recent_events(self, limit: int = 24):
        return [
            {"ts_ms": 1, "code": "BOOT", "value": 0},
            {"ts_ms": 2, "code": "CONFIG_APPLIED", "value": 1},
        ][:limit]

    def debug_vars(self):
        return {
            "transport": {"connected": True},
            "status": self.status(),
            "telemetry": self.latest_telemetry(),
            "events_recent": self.recent_events(),
            "log_store": {"enabled": False},
        }

    def send_ok_command(self, command: str):
        self.commands.append(command)
        return {"ok": True, "command": command}

    def apply_params(self, payload: dict[str, object]):
        self.params_payloads.append(payload)
        return {"ok": True, "payload": payload}


def test_agent_api_endpoints(api_client):
    transport = FakeTransport()
    api_client.app.state.transport = transport

    assert api_client.get("/api/status").json()["axis_state"] == "CONFIG"
    assert api_client.get("/api/telemetry/latest").json()["pos_um"] == 10
    assert len(api_client.get("/api/events/recent?limit=1").json()) == 1
    assert api_client.get("/api/debug/vars").json()["transport"]["connected"] is True

    assert api_client.post("/api/cmd/enable").json()["command"] == "CMD ENABLE"
    assert api_client.post("/api/cmd/ack-fault").json()["command"] == "CMD ACK_FAULT"
    assert api_client.post("/api/cmd/home").json()["command"] == "CMD HOME"
    assert (
        api_client.post("/api/cmd/move-rel", json={"delta_um": 150}).json()["command"]
        == "CMD MOVE_REL 150"
    )
    assert (
        api_client.post("/api/cmd/move-abs", json={"target_um": 2500}).json()["command"]
        == "CMD MOVE_ABS 2500"
    )

    params_rsp = api_client.post(
        "/api/params",
        json={
            "commissioning_stage": 2,
            "safe_mode": 0,
            "arming_only": 1,
            "controlled_motion": 0,
            "max_current": 0.3,
            "max_velocity": 0.005,
            "soft_min_pos": -100,
            "soft_max_pos": 100,
        },
    )
    assert params_rsp.status_code == 200
    assert params_rsp.json()["payload"]["commissioning_stage"] == 2
    assert transport.params_payloads[-1]["soft_max_pos"] == 100


def test_agent_api_maps_firmware_error_to_503(api_client):
    class ErrorTransport(FakeTransport):
        def status(self):
            raise FirmwareError("serial open failed")

    api_client.app.state.transport = ErrorTransport()

    rsp = api_client.get("/api/status")

    assert rsp.status_code == 503
    assert rsp.json()["detail"] == "serial open failed"


def test_agent_api_rejects_unknown_params_field(api_client):
    api_client.app.state.transport = FakeTransport()

    rsp = api_client.post("/api/params", json={"unknown_field": 1})

    assert rsp.status_code == 422


def test_agent_api_validates_move_rel_payload(api_client):
    api_client.app.state.transport = FakeTransport()

    rsp = api_client.post("/api/cmd/move-rel", json={"distance_um": 123})

    assert rsp.status_code == 422
