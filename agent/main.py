from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, ConfigDict

from agent.bridge import FirmwareError, FirmwareTransport


class MoveRelRequest(BaseModel):
    delta_um: int


class MoveAbsRequest(BaseModel):
    target_um: int


class RawCommandRequest(BaseModel):
    command: str


class ParamsRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    commissioning_stage: int | None = None
    safe_mode: int | None = None
    arming_only: int | None = None
    controlled_motion: int | None = None
    brake_installed: int | None = None
    collision_sensor_installed: int | None = None
    ptc_installed: int | None = None
    backup_supply_installed: int | None = None
    external_interlock_installed: int | None = None
    ignore_brake_feedback: int | None = None
    ignore_collision_sensor: int | None = None
    ignore_external_interlock: int | None = None
    allow_motion_without_calibration: int | None = None
    calib_valid: int | None = None
    telemetry_enabled: int | None = None
    max_current: float | None = None
    max_current_peak: float | None = None
    max_velocity: float | None = None
    max_acceleration: float | None = None
    soft_min_pos: int | None = None
    soft_max_pos: int | None = None
    calib_zero_pos: int | None = None
    calib_pitch_um: float | None = None
    calib_sign: int | None = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    transport = FirmwareTransport.from_env()
    app.state.transport = transport
    try:
        yield
    finally:
        transport.close()


app = FastAPI(
    title="Mikrotom STM Agent",
    version="0.1.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def _transport() -> FirmwareTransport:
    return app.state.transport


def _handle_firmware_call(fn):
    try:
        return fn()
    except FirmwareError as exc:
        raise HTTPException(status_code=503, detail=str(exc)) from exc


@app.get("/api/status")
def api_status():
    return _handle_firmware_call(lambda: _transport().status())


@app.get("/api/telemetry/latest")
def api_telemetry_latest():
    return _handle_firmware_call(lambda: _transport().latest_telemetry())


@app.get("/api/events/recent")
def api_events_recent(limit: int = 24):
    return _handle_firmware_call(lambda: _transport().recent_events(limit=limit))


@app.get("/api/debug/vars")
def api_debug_vars():
    return _handle_firmware_call(lambda: _transport().debug_vars())


@app.post("/api/cmd/enable")
def api_cmd_enable():
    return _handle_firmware_call(lambda: _transport().send_ok_command("CMD ENABLE"))


@app.post("/api/cmd/disable")
def api_cmd_disable():
    return _handle_firmware_call(lambda: _transport().send_ok_command("CMD DISABLE"))


@app.post("/api/cmd/stop")
def api_cmd_stop():
    return _handle_firmware_call(lambda: _transport().send_ok_command("CMD STOP"))


@app.post("/api/cmd/qstop")
def api_cmd_qstop():
    return _handle_firmware_call(lambda: _transport().send_ok_command("CMD QSTOP"))


@app.post("/api/cmd/ack-fault")
def api_cmd_ack_fault():
    return _handle_firmware_call(lambda: _transport().send_ok_command("CMD ACK_FAULT"))


@app.post("/api/cmd/home")
def api_cmd_home():
    return _handle_firmware_call(lambda: _transport().send_ok_command("CMD HOME"))


@app.post("/api/cmd/move-rel")
def api_cmd_move_rel(request: MoveRelRequest):
    return _handle_firmware_call(
        lambda: _transport().send_ok_command(f"CMD MOVE_REL {request.delta_um}")
    )


@app.post("/api/cmd/first-move-rel")
def api_cmd_first_move_rel(request: MoveRelRequest):
    return _handle_firmware_call(
        lambda: _transport().send_ok_command(f"CMD FIRST_MOVE_REL {request.delta_um}")
    )


@app.post("/api/cmd/move-abs")
def api_cmd_move_abs(request: MoveAbsRequest):
    return _handle_firmware_call(
        lambda: _transport().send_ok_command(f"CMD MOVE_ABS {request.target_um}")
    )


@app.post("/api/cmd/raw")
def api_cmd_raw(request: RawCommandRequest):
    return _handle_firmware_call(
        lambda: _transport().send_ok_command(request.command.strip())
    )


@app.post("/api/params")
def api_params(request: ParamsRequest):
    payload = request.model_dump()
    return _handle_firmware_call(lambda: _transport().apply_params(payload))
