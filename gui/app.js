const cfg = window.APP_CONFIG || {
  apiBaseUrl: "http://127.0.0.1:8000",
  refreshMs: 300,
  demoMode: true
};

const uiState = {
  status: null,
  telemetry: null,
  events: [],
  history: [],
  connected: false,
  lastError: ""
};

const $ = (id) => document.getElementById(id);

function fmt(value, digits = 0) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) return "--";
  return Number(value).toLocaleString("en-US", {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits
  });
}

function showToast(message, tone = "info") {
  const node = $("toast");
  node.textContent = message;
  node.dataset.tone = tone;
  node.classList.add("visible");
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => node.classList.remove("visible"), 3200);
}

function setBadge(id, text, tone) {
  const node = $(id);
  node.textContent = text;
  node.dataset.tone = tone;
}

async function readJson(response) {
  return await response.json().catch(() => ({}));
}

function extractErrorMessage(body, status) {
  return body.error || body.detail || `HTTP_${status}`;
}

async function apiGet(path) {
  const response = await fetch(`${cfg.apiBaseUrl}${path}`);
  const body = await readJson(response);
  if (!response.ok) throw new Error(extractErrorMessage(body, response.status));
  return body;
}

async function apiPost(path, payload) {
  const response = await fetch(`${cfg.apiBaseUrl}${path}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload || {})
  });
  const body = await readJson(response);
  if (!response.ok) throw new Error(extractErrorMessage(body, response.status));
  return body;
}

async function executeCommand(type, payload = {}) {
  let result;

  if (cfg.demoMode) {
    result = window.demoHandleCommand(type, payload);
  } else {
    switch (type) {
      case "enable":
        result = await apiPost("/api/cmd/enable", {});
        break;
      case "disable":
        result = await apiPost("/api/cmd/disable", {});
        break;
      case "ackFault":
        result = await apiPost("/api/cmd/ack-fault", {});
        break;
      case "stop":
        result = await apiPost("/api/cmd/stop", {});
        break;
      case "qstop":
        result = await apiPost("/api/cmd/qstop", {});
        break;
      case "moveRel":
        result = await apiPost("/api/cmd/move-rel", payload);
        break;
      case "moveAbs":
        result = await apiPost("/api/cmd/move-abs", payload);
        break;
      case "raw":
        result = await apiPost("/api/cmd/raw", payload);
        break;
      case "params":
        result = await apiPost("/api/params", payload);
        break;
      default:
        throw new Error("UNKNOWN_COMMAND");
    }
  }

  if (result && result.ok === false) {
    throw new Error(result.error || "COMMAND_REJECTED");
  }

  await refreshData();
  return result;
}

async function refreshData() {
  try {
    if (cfg.demoMode) {
      window.demoTick();
      uiState.status = JSON.parse(JSON.stringify(window.DEMO_STATE.status));
      uiState.telemetry = JSON.parse(JSON.stringify(window.DEMO_STATE.telemetry));
      uiState.events = JSON.parse(JSON.stringify(window.DEMO_STATE.events));
    } else {
      const [status, telemetry, events] = await Promise.all([
        apiGet("/api/status"),
        apiGet("/api/telemetry/latest"),
        apiGet("/api/events/recent")
      ]);
      uiState.status = status;
      uiState.telemetry = telemetry;
      uiState.events = events || [];
    }

    uiState.connected = true;
    uiState.lastError = "";

    if (uiState.telemetry) {
      uiState.history.push({
        pos: Number(uiState.telemetry.pos_um || 0),
        target: Number(uiState.telemetry.pos_set_um || 0)
      });
      uiState.history = uiState.history.slice(-100);
    }
  } catch (error) {
    uiState.connected = false;
    uiState.lastError = error.message || "UNKNOWN_ERROR";
  }

  renderAll();
}

function stateTone(axisState) {
  switch (String(axisState || "UNKNOWN")) {
    case "FAULT":
    case "ESTOP":
      return "err";
    case "SAFE":
    case "CONFIG":
    case "CALIBRATION":
    case "STOPPING":
    case "ARMED":
      return "warn";
    case "READY":
    case "MOTION":
      return "ok";
    default:
      return "muted";
  }
}

function formatBool(value) {
  return Number(value || 0) === 1 ? "YES" : "NO";
}

function formatEventTime(tsMs) {
  const value = Math.max(0, Number(tsMs || 0));
  if (value < 1000) return `${value} ms`;
  if (value < 60000) return `${(value / 1000).toFixed(2)} s`;
  return `${(value / 60000).toFixed(1)} min`;
}

function explainRunAllowed(status) {
  const blockers = [];
  const calibrationRequired = (
    Number(status.calib_valid || 0) !== 1 &&
    Number(status.allow_motion_without_calibration || 0) !== 1
  );

  if (Number(status.motion_implemented ?? 1) !== 1) blockers.push("this build still has motion disabled");
  if (Number(status.config_loaded || 0) !== 1) blockers.push("configuration was not loaded from flash");
  if (Number(status.fault_mask || 0) !== 0) blockers.push("fault active");
  if (String(status.axis_state || "") === "CONFIG") blockers.push("axis is waiting for configuration or calibration");
  if (Number(status.commissioning_stage || 0) !== 3) blockers.push("stage is not 3");
  if (Number(status.safe_mode || 0) !== 0) blockers.push("safe mode active");
  if (Number(status.enabled || 0) !== 1) blockers.push("drive is not enabled");
  if (calibrationRequired) blockers.push("calibration is required");

  if (Number(status.run_allowed || 0) === 1 && Number(status.motion_implemented ?? 1) === 1) {
    return "Motion is permitted. Use conservative moves and keep STOP/QSTOP ready.";
  }

  return `Motion is blocked because ${blockers.join(", ") || "the backend is not ready"}.`;
}

function renderStatusDetails(status) {
  const items = [
    ["Axis enabled", formatBool(status.enabled)],
    ["Config store loaded", formatBool(status.config_loaded)],
    ["Motion implemented", formatBool(status.motion_implemented)],
    ["Safe integration build", formatBool(status.safe_integration)],
    ["Commissioning stage", status.commissioning_stage ?? "--"],
    ["Safe mode", Number(status.safe_mode || 0) ? "ON" : "OFF"],
    ["Arming only", Number(status.arming_only || 0) ? "ON" : "OFF"],
    ["Controlled motion", Number(status.controlled_motion || 0) ? "ON" : "OFF"],
    ["Allow motion without calibration", formatBool(status.allow_motion_without_calibration)],
    ["Calibration valid", formatBool(status.calib_valid)],
    ["PTC installed", formatBool(status.ptc_installed)],
    ["Backup supply installed", formatBool(status.backup_supply_installed)],
    ["Fault mask", status.fault_mask ?? "--"]
  ];

  $("status-list").innerHTML = items.map(([label, value]) => `
    <div class="status-row">
      <span>${label}</span>
      <strong>${value}</strong>
    </div>
  `).join("");
}

function renderEvents() {
  const body = $("events-body");
  body.innerHTML = "";

  (uiState.events || []).slice(0, 24).forEach((event) => {
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${formatEventTime(event.ts_ms)}</td>
      <td>${event.code || event.type || "--"}</td>
      <td>${event.value ?? event.details ?? "--"}</td>
    `;
    body.appendChild(row);
  });
}

function clamp01(value) {
  return Math.max(0, Math.min(1, value));
}

function mapAxisX(value, min, max, width) {
  if (max <= min) return width / 2;
  const ratio = clamp01((value - min) / (max - min));
  return 78 + ratio * (width - 156);
}

function renderAxis(status, telemetry) {
  const svg = $("axis-svg");
  const width = 1040;
  const height = 220;
  const min = Number(status.soft_min_pos ?? -10000);
  const max = Number(status.soft_max_pos ?? 10000);
  const pos = Number(telemetry.pos_um ?? status.position_um ?? 0);
  const target = Number(telemetry.pos_set_um ?? status.position_set_um ?? 0);
  const runAllowed = Number(status.run_allowed || 0) === 1;
  const fault = String(status.fault || "NONE");
  const color = fault !== "NONE" ? "#ff5e5b" : runAllowed ? "#5b8cff" : "#f0b54a";
  const history = uiState.history.map((point, index) => {
    const x = mapAxisX(point.pos, min, max, width);
    const y = 150 - Math.min(72, index * 0.55);
    return `${x},${y}`;
  }).join(" ");

  const xMin = mapAxisX(min, min, max, width);
  const xMax = mapAxisX(max, min, max, width);
  const xPos = mapAxisX(pos, min, max, width);
  const xTarget = mapAxisX(target, min, max, width);

  svg.innerHTML = `
    <defs>
      <linearGradient id="railGradient" x1="0%" y1="0%" x2="100%" y2="0%">
        <stop offset="0%" stop-color="#f0b54a" />
        <stop offset="100%" stop-color="#ffd17a" />
      </linearGradient>
    </defs>
    <rect x="0" y="0" width="${width}" height="${height}" rx="22" fill="#08111f" />
    <circle cx="${xPos}" cy="112" r="18" fill="${color}" />
    <line x1="${xMin}" y1="112" x2="${xMax}" y2="112" stroke="url(#railGradient)" stroke-width="10" stroke-linecap="round" />
    <line x1="78" y1="112" x2="${xMin}" y2="112" stroke="rgba(255,94,91,0.35)" stroke-width="4" stroke-dasharray="8 7" />
    <line x1="${xMax}" y1="112" x2="${width - 78}" y2="112" stroke="rgba(255,94,91,0.35)" stroke-width="4" stroke-dasharray="8 7" />
    <line x1="${xTarget}" y1="46" x2="${xTarget}" y2="174" stroke="#29c272" stroke-width="4" stroke-dasharray="8 6" />
    ${history ? `<polyline points="${history}" fill="none" stroke="rgba(91,140,255,0.35)" stroke-width="2" />` : ""}
    <text x="${xMin}" y="34" text-anchor="middle" fill="#f6d991" font-size="14">MIN ${fmt(min)} um</text>
    <text x="${xMax}" y="34" text-anchor="middle" fill="#f6d991" font-size="14">MAX ${fmt(max)} um</text>
    <text x="${xPos}" y="194" text-anchor="middle" fill="#f4f7fb" font-size="14">POS ${fmt(pos)} um</text>
    <text x="${xTarget}" y="62" text-anchor="middle" fill="#71e0a4" font-size="14">TARGET ${fmt(target)} um</text>
  `;
}

function renderAll() {
  const status = uiState.status || {};
  const telemetry = uiState.telemetry || {};
  const axisState = status.axis_state || status.state || "UNKNOWN";
  const fault = status.fault || "UNKNOWN";
  const runAllowed = Number(status.run_allowed || 0);
  const stage = Number(status.commissioning_stage || 1);
  const motionImplemented = Number(status.motion_implemented ?? 1);
  const liveMotionReady = runAllowed === 1 && motionImplemented === 1;

  setBadge("badge-connection", uiState.connected ? "Connection OK" : "Connection lost", uiState.connected ? "ok" : "err");
  setBadge("badge-state", `Axis ${axisState}`, stateTone(axisState));
  setBadge("badge-fault", `Fault ${fault}`, fault === "NONE" ? "ok" : "err");
  setBadge("badge-run", `RUN_ALLOWED ${runAllowed}`, liveMotionReady ? "ok" : "warn");
  setBadge("badge-stage", `Stage ${stage}`, stage === 3 ? "ok" : stage === 2 ? "warn" : "muted");

  $("kpi-pos").textContent = `${fmt(telemetry.pos_um ?? status.position_um ?? 0)} um`;
  $("kpi-pos-set").textContent = `${fmt(telemetry.pos_set_um ?? status.position_set_um ?? 0)} um`;
  $("kpi-vel").textContent = `${fmt(telemetry.vel_um_s ?? 0)} um/s`;
  $("kpi-iq").textContent = `${fmt(telemetry.iq_meas_mA ?? 0)} mA`;

  $("cfg-config-loaded").checked = Number(status.config_loaded || 0) === 1;
  $("cfg-enabled").checked = Number(status.enabled || 0) === 1;
  $("cfg-brake-installed").checked = Number(status.brake_installed || 0) === 1;
  $("cfg-collision-installed").checked = Number(status.collision_sensor_installed || 0) === 1;
  $("cfg-external-interlock-installed").checked = Number(status.external_interlock_installed || 0) === 1;
  $("cfg-ignore-brake").checked = Number(status.ignore_brake_feedback || 0) === 1;
  $("cfg-ignore-collision").checked = Number(status.ignore_collision_sensor || 0) === 1;
  $("cfg-ignore-interlock").checked = Number(status.ignore_external_interlock || 0) === 1;
  $("cfg-allow-no-calib").checked = Number(status.allow_motion_without_calibration || 0) === 1;
  $("cfg-calib-valid").checked = Number(status.calib_valid || 0) === 1;
  $("cfg-max-current").value = status.max_current ?? 0.2;
  $("cfg-max-current-peak").value = status.max_current_peak ?? 0.3;
  $("cfg-max-velocity").value = status.max_velocity ?? 0.005;
  $("cfg-max-acceleration").value = status.max_acceleration ?? 0.02;
  $("cfg-soft-min").value = status.soft_min_pos ?? -10000;
  $("cfg-soft-max").value = status.soft_max_pos ?? 10000;

  $("move-warning").textContent = explainRunAllowed(status);
  $("connection-detail").textContent = uiState.connected
    ? `Source: ${cfg.demoMode ? "DEMO data model" : cfg.apiBaseUrl} | build ${motionImplemented ? "motion-enabled" : "safe-integration"}`
    : `Source unavailable: ${uiState.lastError || "unknown error"}`;

  $("btn-move-rel").disabled = !liveMotionReady;
  $("btn-move-abs").disabled = !liveMotionReady;
  $("btn-ack-fault").disabled = Number(status.fault_mask || 0) === 0;

  ["stage-1-card", "stage-2-card", "stage-3-card"].forEach((id, index) => {
    $(id).classList.toggle("active", stage === index + 1);
  });

  renderStatusDetails(status);
  renderAxis(status, telemetry);
  renderEvents();

  $("footer-left").textContent = `API ${cfg.apiBaseUrl}`;
  $("footer-right").textContent = `Mode ${cfg.demoMode ? "DEMO" : "LIVE"} | ${motionImplemented ? "MOTION" : "SAFE-INTEGRATION"}`;
}

function stageChecksSatisfied(stage) {
  const boxes = [...document.querySelectorAll(`.stage${stage}-check`)];
  return boxes.length > 0 && boxes.every((box) => box.checked);
}

async function runAction(action, payload, successMessage) {
  try {
    await executeCommand(action, payload);
    showToast(successMessage, "ok");
  } catch (error) {
    showToast(error.message || "Command failed", "err");
  }
}

function bindActions() {
  $("btn-enable").addEventListener("click", () => runAction("enable", {}, "ENABLE sent"));
  $("btn-disable").addEventListener("click", () => runAction("disable", {}, "DISABLE sent"));
  $("btn-ack-fault").addEventListener("click", () => runAction("ackFault", {}, "ACK_FAULT sent"));
  $("btn-stop").addEventListener("click", () => runAction("stop", {}, "STOP sent"));
  $("btn-qstop").addEventListener("click", () => runAction("qstop", {}, "QSTOP sent"));

  $("btn-move-rel").addEventListener("click", () => runAction("moveRel", {
    delta_um: Number($("move-rel").value || 0)
  }, "MOVE_REL sent"));

  $("btn-move-abs").addEventListener("click", () => runAction("moveAbs", {
    target_um: Number($("move-abs").value || 0)
  }, "MOVE_ABS sent"));

  $("btn-calib-zero").addEventListener("click", () => runAction("raw", {
    command: "CMD CALIB_ZERO"
  }, "CALIB_ZERO sent"));

  $("btn-send-raw").addEventListener("click", () => runAction("raw", {
    command: $("raw-command").value
  }, "RAW command sent"));

  $("btn-refresh").addEventListener("click", async () => {
    await refreshData();
    showToast("Data refreshed", "info");
  });

  $("btn-stage-1").addEventListener("click", async () => {
    if (!stageChecksSatisfied(1)) {
      showToast("Complete Stage 1 checklist first", "warn");
      return;
    }
    await runAction("params", { commissioning_stage: 1 }, "Stage 1 activated");
  });

  $("btn-stage-2").addEventListener("click", async () => {
    if (!stageChecksSatisfied(2)) {
      showToast("Complete Stage 2 checklist first", "warn");
      return;
    }
    await runAction("params", { commissioning_stage: 2 }, "Stage 2 activated");
  });

  $("btn-stage-3").addEventListener("click", async () => {
    if (!stageChecksSatisfied(3)) {
      showToast("Complete Stage 3 checklist first", "warn");
      return;
    }
    await runAction("params", { commissioning_stage: 3 }, "Stage 3 activated");
  });

  $("btn-save-safety").addEventListener("click", async () => {
    await runAction("params", {
      brake_installed: $("cfg-brake-installed").checked ? 1 : 0,
      collision_sensor_installed: $("cfg-collision-installed").checked ? 1 : 0,
      external_interlock_installed: $("cfg-external-interlock-installed").checked ? 1 : 0,
      ignore_brake_feedback: $("cfg-ignore-brake").checked ? 1 : 0,
      ignore_collision_sensor: $("cfg-ignore-collision").checked ? 1 : 0,
      ignore_external_interlock: $("cfg-ignore-interlock").checked ? 1 : 0,
      allow_motion_without_calibration: $("cfg-allow-no-calib").checked ? 1 : 0,
      max_current: Number($("cfg-max-current").value || 0.2),
      max_current_peak: Number($("cfg-max-current-peak").value || 0.3),
      max_velocity: Number($("cfg-max-velocity").value || 0.005),
      max_acceleration: Number($("cfg-max-acceleration").value || 0.02),
      soft_min_pos: Number($("cfg-soft-min").value || -10000),
      soft_max_pos: Number($("cfg-soft-max").value || 10000)
    }, "Safety parameters sent");
  });
}

async function bootstrap() {
  bindActions();
  await refreshData();
  setInterval(refreshData, cfg.refreshMs || 300);
}

bootstrap();
