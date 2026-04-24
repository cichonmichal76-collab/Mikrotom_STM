window.DEMO_STATE = {
  stop_pending: 0,
  status: {
    axis_state: "CONFIG",
    fault: "NONE",
    fault_mask: 0,
    commissioning_stage: 1,
    safe_mode: 1,
    arming_only: 0,
    controlled_motion: 0,
    run_allowed: 0,
    enabled: 0,
    config_loaded: 0,
    safe_integration: 0,
    motion_implemented: 1,
    position_um: 0,
    position_set_um: 0,
    vbus_V: 12.0,
    vbus_valid: 1,
    brake_installed: 0,
    collision_sensor_installed: 0,
    ptc_installed: 1,
    backup_supply_installed: 0,
    external_interlock_installed: 0,
    ignore_brake_feedback: 0,
    ignore_collision_sensor: 0,
    ignore_external_interlock: 0,
    allow_motion_without_calibration: 0,
    calib_valid: 0,
    max_current: 0.2,
    max_current_peak: 0.3,
    max_velocity: 0.005,
    max_acceleration: 0.02,
    soft_min_pos: -10000,
    soft_max_pos: 10000,
    calib_zero_pos: 0,
    calib_pitch_um: 30000,
    calib_sign: 1
  },
  telemetry: {
    ts_ms: 0,
    pos_um: 0,
    pos_set_um: 0,
    vel_um_s: 0,
    vel_set_um_s: 0,
    iq_ref_mA: 0,
    iq_meas_mA: 0,
    state: "CONFIG",
    fault: 0
  },
  events: [
    { ts_ms: 0, code: "BOOT", value: 0 }
  ]
};

window.demoBootTs = Date.now();

window.demoPushEvent = function(code, value) {
  window.DEMO_STATE.events.unshift({
    ts_ms: Date.now() - window.demoBootTs,
    code,
    value: value ?? 0
  });
  window.DEMO_STATE.events = window.DEMO_STATE.events.slice(0, 64);
};

window.demoRecompute = function() {
  const status = window.DEMO_STATE.status;
  const calibrationRequired = status.calib_valid !== 1 && status.allow_motion_without_calibration !== 1;

  status.safe_mode = status.commissioning_stage === 1 ? 1 : 0;
  status.arming_only = status.commissioning_stage === 2 ? 1 : 0;
  status.controlled_motion = status.commissioning_stage === 3 ? 1 : 0;

  status.run_allowed = (
    status.fault_mask === 0 &&
    status.controlled_motion === 1 &&
    status.safe_mode === 0 &&
    status.enabled === 1 &&
    calibrationRequired === false &&
    status.motion_implemented === 1
  ) ? 1 : 0;

  if (status.fault_mask !== 0 || status.fault !== "NONE") {
    status.axis_state = "FAULT";
    return;
  }

  if (status.config_loaded !== 1 || calibrationRequired) {
    status.axis_state = "CONFIG";
    return;
  }

  if (status.safe_mode === 1 || status.enabled !== 1) {
    status.axis_state = "SAFE";
    return;
  }

  if (status.arming_only === 1) {
    status.axis_state = "ARMED";
    return;
  }

  status.axis_state = "READY";
};

window.demoClampTarget = function(target) {
  const status = window.DEMO_STATE.status;
  if (target < status.soft_min_pos || target > status.soft_max_pos) return null;
  return target;
};

window.demoTick = function() {
  const status = window.DEMO_STATE.status;
  const telemetry = window.DEMO_STATE.telemetry;
  const now = Date.now() - window.demoBootTs;
  const delta = status.position_set_um - status.position_um;

  window.demoRecompute();

  if (window.DEMO_STATE.stop_pending) {
    window.DEMO_STATE.stop_pending = 0;
    status.axis_state = "STOPPING";
    telemetry.vel_um_s = 0;
    telemetry.iq_ref_mA = 0;
    telemetry.iq_meas_mA = 0;
  } else if (status.run_allowed && Math.abs(delta) > 2) {
    const step = Math.sign(delta) * Math.min(Math.abs(delta), 120);
    status.axis_state = "MOTION";
    status.position_um += step;
    telemetry.vel_um_s = step * 8;
    telemetry.iq_ref_mA = Math.min(500, Math.abs(step) * 3);
    telemetry.iq_meas_mA = Math.min(520, Math.abs(step) * 3 + 20);
  } else {
    telemetry.vel_um_s = 0;
    telemetry.iq_ref_mA = 0;
    telemetry.iq_meas_mA = 0;
  }

  telemetry.ts_ms = now;
  status.vbus_V = 12.0 + Math.sin(now / 1200) * 0.08;
  telemetry.pos_um = Math.round(status.position_um);
  telemetry.pos_set_um = Math.round(status.position_set_um);
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
      window.demoPushEvent("ENABLE", 0);
      window.demoRecompute();
      return { ok: true };
    case "disable":
      status.enabled = 0;
      status.position_set_um = status.position_um;
      window.demoPushEvent("DISABLE", 0);
      window.demoRecompute();
      return { ok: true };
    case "ackFault":
      status.fault = "NONE";
      status.fault_mask = 0;
      window.demoPushEvent("FAULT_ACK", 0);
      window.demoRecompute();
      return { ok: true };
    case "stop":
      status.position_set_um = status.position_um;
      window.DEMO_STATE.stop_pending = 1;
      window.demoPushEvent("STOP", 0);
      return { ok: true };
    case "qstop":
      status.enabled = 0;
      status.position_set_um = status.position_um;
      status.axis_state = "SAFE";
      window.demoPushEvent("QSTOP", 0);
      window.demoRecompute();
      return { ok: true };
    case "moveRel": {
      window.demoRecompute();
      if (!status.run_allowed) return { ok: false, error: "RUN_NOT_ALLOWED" };
      const target = window.demoClampTarget(status.position_set_um + Number(payload?.delta_um || 0));
      if (target === null) return { ok: false, error: "LIMIT_REJECTED" };
      status.position_set_um = target;
      window.demoPushEvent("MOVE_REL", Number(payload?.delta_um || 0));
      return { ok: true };
    }
    case "moveAbs": {
      window.demoRecompute();
      if (!status.run_allowed) return { ok: false, error: "RUN_NOT_ALLOWED" };
      const target = window.demoClampTarget(Number(payload?.target_um || 0));
      if (target === null) return { ok: false, error: "LIMIT_REJECTED" };
      status.position_set_um = target;
      window.demoPushEvent("MOVE_ABS", target);
      return { ok: true };
    }
    case "raw":
      if (raw === "CMD CALIB_ZERO") {
        status.calib_valid = 1;
        status.calib_zero_pos = Math.round(status.position_um);
        status.config_loaded = 1;
        window.demoPushEvent("CALIB_OK", Math.round(status.position_um));
        window.demoRecompute();
        return { ok: true };
      }
      if (raw === "CMD ACK_FAULT") {
        return window.demoHandleCommand("ackFault", {});
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
        if (key in status) {
          status[key] = typeof status[key] === "number" ? Number(value) : value;
        }
      });
      status.config_loaded = 1;
      if (Object.prototype.hasOwnProperty.call(payload || {}, "commissioning_stage")) {
        window.demoPushEvent("STAGE_CHANGED", Number(payload.commissioning_stage));
      } else {
        window.demoPushEvent("CONFIG_SAVED", 0);
      }
      window.demoRecompute();
      return { ok: true };
    default:
      return { ok: false, error: "UNKNOWN_DEMO_COMMAND" };
  }
};
