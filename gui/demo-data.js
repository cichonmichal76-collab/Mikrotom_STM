window.DEMO_STATE = {
  status: {
    axis_state: "SAFE",
    fault: "NONE",
    fault_mask: 0,
    commissioning_stage: 1,
    safe_mode: 1,
    arming_only: 0,
    controlled_motion: 0,
    run_allowed: 0,
    enabled: 0,
    position_um: 0,
    position_set_um: 0,
    brake_installed: 0,
    ignore_brake_feedback: 0,
    calib_valid: 0,
    max_current: 0.2,
    max_velocity: 0.005,
    soft_min_pos: -10000,
    soft_max_pos: 10000
  },
  telemetry: {
    ts_ms: 0,
    pos_um: 0,
    pos_set_um: 0,
    vel_um_s: 0,
    vel_set_um_s: 0,
    iq_ref_mA: 0,
    iq_meas_mA: 0,
    state: 1,
    fault: 0
  },
  events: [
    { ts_ms: 0, code: "BOOT", value: 0 }
  ]
};

window.demoPushEvent = function(code, value) {
  window.DEMO_STATE.events.unshift({
    ts_ms: Date.now(),
    code: code,
    value: value ?? 0
  });
  window.DEMO_STATE.events = window.DEMO_STATE.events.slice(0, 64);
};

window.demoRecompute = function() {
  const status = window.DEMO_STATE.status;
  status.safe_mode = status.commissioning_stage === 1 ? 1 : 0;
  status.arming_only = status.commissioning_stage === 2 ? 1 : 0;
  status.controlled_motion = status.commissioning_stage === 3 ? 1 : 0;
  status.run_allowed = (
    status.fault_mask === 0 &&
    status.commissioning_stage === 3 &&
    status.safe_mode === 0 &&
    status.calib_valid === 1 &&
    status.enabled === 1
  ) ? 1 : 0;
};

window.demoTick = function() {
  const status = window.DEMO_STATE.status;
  const telemetry = window.DEMO_STATE.telemetry;
  const now = Date.now();
  const delta = status.position_set_um - status.position_um;

  window.demoRecompute();

  if (status.run_allowed && Math.abs(delta) > 2) {
    const step = Math.sign(delta) * Math.min(Math.abs(delta), 120);
    status.axis_state = "MOTION";
    status.position_um += step;
    telemetry.vel_um_s = step * 8;
    telemetry.iq_ref_mA = Math.min(500, Math.abs(step) * 3);
    telemetry.iq_meas_mA = Math.min(520, Math.abs(step) * 3 + 20);
  } else {
    if (status.fault !== "NONE") {
      status.axis_state = "FAULT";
    } else if (status.enabled && status.commissioning_stage >= 2) {
      status.axis_state = "READY";
    } else {
      status.axis_state = "SAFE";
    }
    status.position_um = Math.round(status.position_um);
    telemetry.vel_um_s = 0;
    telemetry.iq_ref_mA = 0;
    telemetry.iq_meas_mA = 0;
  }

  telemetry.ts_ms = now;
  telemetry.pos_um = status.position_um;
  telemetry.pos_set_um = status.position_set_um;
  telemetry.vel_set_um_s = 0;
  telemetry.state = status.axis_state;
  telemetry.fault = status.fault_mask;
};

window.demoHandleCommand = function(type, payload) {
  const status = window.DEMO_STATE.status;
  const raw = String(payload?.command || "").trim();

  switch (type) {
    case "enable":
      if (status.fault_mask !== 0) return { ok: false, error: "FAULT_ACTIVE" };
      status.enabled = 1;
      status.axis_state = status.commissioning_stage >= 2 ? "READY" : "SAFE";
      window.demoPushEvent("ENABLE", 0);
      window.demoRecompute();
      return { ok: true };
    case "disable":
      status.enabled = 0;
      status.axis_state = "SAFE";
      status.position_set_um = status.position_um;
      window.demoPushEvent("DISABLE", 0);
      window.demoRecompute();
      return { ok: true };
    case "stop":
      status.position_set_um = status.position_um;
      status.axis_state = "STOPPING";
      window.demoPushEvent("STOP", 0);
      return { ok: true };
    case "qstop":
      status.enabled = 0;
      status.position_set_um = status.position_um;
      status.axis_state = "SAFE";
      window.demoPushEvent("QSTOP", 0);
      window.demoRecompute();
      return { ok: true };
    case "moveRel":
      window.demoRecompute();
      if (!status.run_allowed) return { ok: false, error: "RUN_NOT_ALLOWED" };
      status.position_set_um += Number(payload?.delta_um || 0);
      window.demoPushEvent("MOVE_REL", Number(payload?.delta_um || 0));
      return { ok: true };
    case "moveAbs":
      window.demoRecompute();
      if (!status.run_allowed) return { ok: false, error: "RUN_NOT_ALLOWED" };
      status.position_set_um = Number(payload?.target_um || 0);
      window.demoPushEvent("MOVE_ABS", Number(payload?.target_um || 0));
      return { ok: true };
    case "raw":
      if (raw === "CMD CALIB_ZERO") {
        status.calib_valid = 1;
        window.demoPushEvent("CALIB_OK", status.position_um);
        window.demoRecompute();
        return { ok: true };
      }
      if (raw.startsWith("CMD MOVE_REL ")) {
        return window.demoHandleCommand("moveRel", { delta_um: Number(raw.slice(13)) });
      }
      if (raw.startsWith("CMD MOVE_ABS ")) {
        return window.demoHandleCommand("moveAbs", { target_um: Number(raw.slice(13)) });
      }
      window.demoPushEvent("RAW", raw);
      return { ok: true };
    case "params":
      Object.entries(payload || {}).forEach(([key, value]) => {
        if (key === "commissioning_stage") {
          status.commissioning_stage = Number(value);
          window.demoPushEvent("STAGE_CHANGED", Number(value));
        } else if (key in status) {
          status[key] = typeof status[key] === "number" ? Number(value) : value;
        }
      });
      window.demoRecompute();
      return { ok: true };
    default:
      return { ok: false, error: "UNKNOWN_DEMO_COMMAND" };
  }
};
