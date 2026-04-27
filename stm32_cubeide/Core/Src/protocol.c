#include "protocol.h"
#include "main.h"
#include "usart.h"
#include "axis_state.h"
#include "fault.h"
#include "commissioning.h"
#include "limits.h"
#include "safety_config.h"
#include "fw_version.h"
#include "calibration.h"
#include "axis_control.h"
#include "watchdogs.h"
#include "eventlog.h"
#include "motorState.h"
#include "trajectory.h"
#include "config_store.h"
#include "app_build_config.h"
#include "telemetry.h"
#include "safety_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <float.h>

#define PROTOCOL_RX_BUFFER_SIZE 128u
#define PROTOCOL_CMD_QUEUE_DEPTH 4u

#define PROTOCOL_PENDING_ERR_RX_OVERFLOW (1u << 0)
#define PROTOCOL_PENDING_ERR_CMD_OVERFLOW (1u << 1)

extern volatile MotorState state;
extern volatile MotorTrajectory traj;

static uint8_t g_rx_byte = 0u;
static char g_rx_buffer[PROTOCOL_RX_BUFFER_SIZE];
static char g_cmd_queue[PROTOCOL_CMD_QUEUE_DEPTH][PROTOCOL_RX_BUFFER_SIZE];
static uint16_t g_rx_index = 0u;
static volatile uint8_t g_cmd_head = 0u;
static volatile uint8_t g_cmd_tail = 0u;
static volatile uint8_t g_cmd_count = 0u;
static volatile uint8_t g_pending_errors = 0u;
static volatile uint8_t g_rx_active = 0u;

uint8_t Protocol_GetLastRxByte(void){ return g_rx_byte; }

void Protocol_SendLine(const char *line)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)line, (uint16_t)strlen(line), 100);
}

static void protocol_rsp_ok(void){ Protocol_SendLine("RSP,OK\n"); }
static void protocol_rsp_err(const char *reason){ char msg[96]; snprintf(msg,sizeof(msg),"RSP,ERR,%s\n",reason); Protocol_SendLine(msg); }
static void protocol_rsp_param_f(const char *name, float value){ char msg[128]; snprintf(msg,sizeof(msg),"RSP,PARAM,%s,%.6f\n",name,value); Protocol_SendLine(msg); }
static void protocol_rsp_param_i32(const char *name, int32_t value){ char msg[128]; snprintf(msg,sizeof(msg),"RSP,PARAM,%s,%ld\n",name,(long)value); Protocol_SendLine(msg); }
static void protocol_rsp_param_u32(const char *name, uint32_t value){ char msg[128]; snprintf(msg,sizeof(msg),"RSP,PARAM,%s,%lu\n",name,(unsigned long)value); Protocol_SendLine(msg); }
static void protocol_rsp_config_u8(const char *name, uint8_t value){ char msg[128]; snprintf(msg,sizeof(msg),"RSP,CONFIG,%s,%u\n",name,(unsigned)value); Protocol_SendLine(msg); }

static uint8_t protocol_parse_i32(const char *value, int32_t *out)
{
    char *end = 0;
    long parsed;

    if ((value == 0) || (out == 0) || (*value == '\0'))
        return 0u;

    errno = 0;
    parsed = strtol(value, &end, 10);
    if ((errno != 0) || (end == value) || (*end != '\0'))
        return 0u;

    if ((parsed < (long)INT32_MIN) || (parsed > (long)INT32_MAX))
        return 0u;

    *out = (int32_t)parsed;
    return 1u;
}

static uint8_t protocol_parse_float(const char *value, float *out)
{
    char *end = 0;
    float parsed;

    if ((value == 0) || (out == 0) || (*value == '\0'))
        return 0u;

    errno = 0;
    parsed = strtof(value, &end);
    if ((errno != 0) || (end == value) || (*end != '\0'))
        return 0u;
    if (!(parsed <= FLT_MAX) || !(parsed >= -FLT_MAX))
        return 0u;

    *out = parsed;
    return 1u;
}

static uint8_t protocol_parse_bool(const char *value, uint8_t *out)
{
    int32_t parsed;

    if (!protocol_parse_i32(value, &parsed))
        return 0u;
    if ((parsed != 0) && (parsed != 1))
        return 0u;

    *out = (uint8_t)parsed;
    return 1u;
}

static void protocol_queue_error(uint8_t err_mask)
{
    g_pending_errors |= err_mask;
}

static void protocol_rsp_saved_ok(void)
{
    if (ConfigStore_Save()) protocol_rsp_ok();
    else protocol_rsp_err("SAVE_FAILED");
}

static void protocol_rsp_event(const EventEntry_t *entry)
{
    char msg[160];

    if (entry == 0)
    {
        Protocol_SendLine("RSP,EVENT,NONE\n");
        return;
    }

    snprintf(msg,
             sizeof(msg),
             "RSP,EVENT,%lu,%s,%ld\n",
             (unsigned long)entry->ts_ms,
             EventLog_CodeName(entry->code),
             (long)entry->value);
    Protocol_SendLine(msg);
}

static void protocol_queue_completed_command(void)
{
    if (g_cmd_count >= PROTOCOL_CMD_QUEUE_DEPTH)
    {
        protocol_queue_error(PROTOCOL_PENDING_ERR_CMD_OVERFLOW);
        return;
    }

    memcpy(g_cmd_queue[g_cmd_head], g_rx_buffer, (size_t)g_rx_index + 1u);
    g_cmd_head = (uint8_t)((g_cmd_head + 1u) % PROTOCOL_CMD_QUEUE_DEPTH);
    g_cmd_count++;
}

void Protocol_Init(void)
{
    g_rx_index = 0u;
    g_cmd_head = 0u;
    g_cmd_tail = 0u;
    g_cmd_count = 0u;
    g_pending_errors = 0u;
    g_rx_active = 0u;
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    memset(g_cmd_queue, 0, sizeof(g_cmd_queue));
    Protocol_EnsureRxActive();
}

void Protocol_EnsureRxActive(void)
{
    if (!g_rx_active)
    {
        if (HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1) == HAL_OK)
            g_rx_active = 1u;
    }
}

void Protocol_RxErrorCallback(void)
{
    g_rx_active = 0u;
    g_rx_index = 0u;
    __HAL_UART_CLEAR_OREFLAG(&huart2);
    (void)HAL_UART_AbortReceive_IT(&huart2);
    Protocol_EnsureRxActive();
}

void Protocol_RxCpltCallback(uint8_t byte)
{
    g_rx_active = 0u;
    if ((byte == '\n') || (byte == '\r'))
    {
        if (g_rx_index > 0u)
        {
            g_rx_buffer[g_rx_index] = '\0';
            protocol_queue_completed_command();
        }
        g_rx_index = 0u;
    }
    else
    {
        if (g_rx_index < (PROTOCOL_RX_BUFFER_SIZE - 1u))
            g_rx_buffer[g_rx_index++] = (char)byte;
        else
        {
            g_rx_index = 0u;
            protocol_queue_error(PROTOCOL_PENDING_ERR_RX_OVERFLOW);
        }
    }
    Protocol_EnsureRxActive();
}

static void handle_get_simple(const char *arg1)
{
    if (arg1 == NULL) { protocol_rsp_err("GET_FORMAT"); return; }

    if (strcmp(arg1, "STATE") == 0)
    {
        char msg[96];
        snprintf(msg,sizeof(msg),"RSP,STATE,%s\n",AxisState_Name(AxisState_Get()));
        Protocol_SendLine(msg);
    }
    else if (strcmp(arg1, "FAULT") == 0)
    {
        char msg[96];
        snprintf(msg,sizeof(msg),"RSP,FAULT,%s\n",Fault_Name(Fault_GetLast()));
        Protocol_SendLine(msg);
    }
    else if (strcmp(arg1, "FAULT_MASK") == 0)
        protocol_rsp_param_u32("FAULT_MASK", Fault_GetMask());
    else if (strcmp(arg1, "COMMISSION_STAGE") == 0)
        protocol_rsp_param_u32("COMMISSION_STAGE", Commissioning_GetStage());
    else if (strcmp(arg1, "SAFE_MODE") == 0)
        protocol_rsp_config_u8("SAFE_MODE", Commissioning_SafeMode());
    else if (strcmp(arg1, "ARMING_ONLY") == 0)
        protocol_rsp_config_u8("ARMING_ONLY", Commissioning_ArmingOnly());
    else if (strcmp(arg1, "CONTROLLED_MOTION") == 0)
        protocol_rsp_config_u8("CONTROLLED_MOTION", Commissioning_ControlledMotion());
    else if (strcmp(arg1, "RUN_ALLOWED") == 0)
        protocol_rsp_config_u8("RUN_ALLOWED", AxisControl_RunAllowed());
    else if (strcmp(arg1, "ENABLED") == 0)
        protocol_rsp_config_u8("ENABLED", state.enabled ? 1u : 0u);
    else if (strcmp(arg1, "HOMING_STARTED") == 0)
        protocol_rsp_config_u8("HOMING_STARTED", state.HomingStarted ? 1u : 0u);
    else if (strcmp(arg1, "HOMING_SUCCESSFUL") == 0)
        protocol_rsp_config_u8("HOMING_SUCCESSFUL", state.HomingSuccessful ? 1u : 0u);
    else if (strcmp(arg1, "HOMING_ONGOING") == 0)
        protocol_rsp_config_u8("HOMING_ONGOING", state.HomingOngoing ? 1u : 0u);
    else if (strcmp(arg1, "FIRST_MOVE_TEST_ACTIVE") == 0)
        protocol_rsp_config_u8("FIRST_MOVE_TEST_ACTIVE", AxisControl_FirstMoveTestActive());
    else if (strcmp(arg1, "FIRST_MOVE_MAX_DELTA") == 0)
        protocol_rsp_param_i32("FIRST_MOVE_MAX_DELTA", 100);
    else if (strcmp(arg1, "TELEMETRY_ENABLED") == 0)
        protocol_rsp_config_u8("TELEMETRY_ENABLED", Telemetry_IsEnabled());
    else if (strcmp(arg1, "CONFIG_LOADED") == 0)
        protocol_rsp_config_u8("CONFIG_LOADED", ConfigStore_IsLoaded());
    else if (strcmp(arg1, "SAFE_INTEGRATION") == 0)
        protocol_rsp_config_u8("SAFE_INTEGRATION", APP_SAFE_INTEGRATION ? 1u : 0u);
    else if (strcmp(arg1, "MOTION_IMPLEMENTED") == 0)
        protocol_rsp_config_u8("MOTION_IMPLEMENTED", APP_MOTION_IMPLEMENTED ? 1u : 0u);
    else if (strcmp(arg1, "POS") == 0)
        protocol_rsp_param_i32("POS", (int32_t)state.pos_um);
    else if (strcmp(arg1, "VBUS") == 0)
        protocol_rsp_param_f("VBUS", state.Vbus);
    else if (strcmp(arg1, "VBUS_VALID") == 0)
        protocol_rsp_config_u8("VBUS_VALID", state.Vbus_valid ? 1u : 0u);
    else if (strcmp(arg1, "EVENT_COUNT") == 0)
        protocol_rsp_param_u32("EVENT_COUNT", EventLog_Count());
    else if (strcmp(arg1, "EVENT_POP") == 0)
    {
        EventEntry_t entry;
        if (EventLog_Pop(&entry)) protocol_rsp_event(&entry);
        else protocol_rsp_event(0);
    }
    else if (strcmp(arg1, "VERSION") == 0)
    {
        char msg[192];
        snprintf(msg,sizeof(msg),"RSP,VERSION,%s,%s,%s,%s\n",FwVersion_Name(),FwVersion_Version(),FwVersion_BuildDate(),FwVersion_GitHash());
        Protocol_SendLine(msg);
    }
    else protocol_rsp_err("GET_UNKNOWN");
}

static void handle_get_param(const char *name)
{
    const Limits_t *lim = Limits_Get();
    if (name == NULL) { protocol_rsp_err("GET_PARAM_FORMAT"); return; }
    if (strcmp(name, "MAX_CURRENT") == 0) protocol_rsp_param_f("MAX_CURRENT", lim->max_current_nominal_A);
    else if (strcmp(name, "MAX_CURRENT_PEAK") == 0) protocol_rsp_param_f("MAX_CURRENT_PEAK", lim->max_current_peak_A);
    else if (strcmp(name, "MAX_VELOCITY") == 0) protocol_rsp_param_f("MAX_VELOCITY", lim->max_velocity_m_s);
    else if (strcmp(name, "MAX_ACCELERATION") == 0) protocol_rsp_param_f("MAX_ACCELERATION", lim->max_acceleration_m_s2);
    else if (strcmp(name, "SOFT_MIN_POS") == 0) protocol_rsp_param_i32("SOFT_MIN_POS", lim->soft_min_pos_um);
    else if (strcmp(name, "SOFT_MAX_POS") == 0) protocol_rsp_param_i32("SOFT_MAX_POS", lim->soft_max_pos_um);
    else if (strcmp(name, "CALIB_ZERO_POS") == 0) protocol_rsp_param_i32("CALIB_ZERO_POS", Calibration_GetZeroPosUm());
    else if (strcmp(name, "CALIB_PITCH_UM") == 0) protocol_rsp_param_f("CALIB_PITCH_UM", Calibration_GetPitchUm());
    else if (strcmp(name, "CALIB_SIGN") == 0) protocol_rsp_param_i32("CALIB_SIGN", Calibration_GetSign());
    else if (strcmp(name, "CURRENT_U") == 0) protocol_rsp_param_f("CURRENT_U", state.current_U);
    else if (strcmp(name, "CURRENT_V") == 0) protocol_rsp_param_f("CURRENT_V", state.current_V);
    else if (strcmp(name, "CURRENT_W") == 0) protocol_rsp_param_f("CURRENT_W", state.current_W);
    else if (strcmp(name, "CURRENT_U_RAW") == 0) protocol_rsp_param_f("CURRENT_U_RAW", state.current_U_raw);
    else if (strcmp(name, "CURRENT_V_RAW") == 0) protocol_rsp_param_f("CURRENT_V_RAW", state.current_V_raw);
    else if (strcmp(name, "CURRENT_U_ADC") == 0) protocol_rsp_param_i32("CURRENT_U_ADC", state.current_U_ADC);
    else if (strcmp(name, "CURRENT_V_ADC") == 0) protocol_rsp_param_i32("CURRENT_V_ADC", state.current_V_ADC);
    else if (strcmp(name, "CURRENT_U_ADC_OFFSET") == 0) protocol_rsp_param_i32("CURRENT_U_ADC_OFFSET", state.current_U_ADC_offset);
    else if (strcmp(name, "CURRENT_V_ADC_OFFSET") == 0) protocol_rsp_param_i32("CURRENT_V_ADC_OFFSET", state.current_V_ADC_offset);
    else if (strcmp(name, "CURRENT_MAX") == 0) protocol_rsp_param_f("CURRENT_MAX", state.maxValCurr);
    else if (strcmp(name, "CURRENT_MIN") == 0) protocol_rsp_param_f("CURRENT_MIN", state.minValCurr);
    else protocol_rsp_err("GET_PARAM_UNKNOWN");
}

static void handle_get_config(const char *name)
{
    const SafetyConfig_t *cfg = SafetyConfig_Get();
    if (name == NULL) { protocol_rsp_err("GET_CONFIG_FORMAT"); return; }
    if (strcmp(name, "BRAKE_INSTALLED") == 0) protocol_rsp_config_u8("BRAKE_INSTALLED", cfg->brake_installed);
    else if (strcmp(name, "COLLISION_SENSOR_INSTALLED") == 0) protocol_rsp_config_u8("COLLISION_SENSOR_INSTALLED", cfg->collision_sensor_installed);
    else if (strcmp(name, "PTC_INSTALLED") == 0) protocol_rsp_config_u8("PTC_INSTALLED", cfg->ptc_installed);
    else if (strcmp(name, "BACKUP_SUPPLY_INSTALLED") == 0) protocol_rsp_config_u8("BACKUP_SUPPLY_INSTALLED", cfg->backup_supply_installed);
    else if (strcmp(name, "EXTERNAL_INTERLOCK_INSTALLED") == 0) protocol_rsp_config_u8("EXTERNAL_INTERLOCK_INSTALLED", cfg->external_interlock_installed);
    else if (strcmp(name, "IGNORE_BRAKE_FEEDBACK") == 0) protocol_rsp_config_u8("IGNORE_BRAKE_FEEDBACK", cfg->ignore_brake_feedback);
    else if (strcmp(name, "IGNORE_COLLISION_SENSOR") == 0) protocol_rsp_config_u8("IGNORE_COLLISION_SENSOR", cfg->ignore_collision_sensor);
    else if (strcmp(name, "IGNORE_EXTERNAL_INTERLOCK") == 0) protocol_rsp_config_u8("IGNORE_EXTERNAL_INTERLOCK", cfg->ignore_external_interlock);
    else if (strcmp(name, "ALLOW_MOTION_WITHOUT_CALIBRATION") == 0) protocol_rsp_config_u8("ALLOW_MOTION_WITHOUT_CALIBRATION", cfg->allow_motion_without_calibration);
    else if (strcmp(name, "CALIB_VALID") == 0) protocol_rsp_config_u8("CALIB_VALID", Calibration_IsValid());
    else protocol_rsp_err("GET_CONFIG_UNKNOWN");
}

static void handle_set_param(const char *name, const char *value)
{
    Limits_t next_limits;

    if ((name == NULL) || (value == NULL)) { protocol_rsp_err("SET_PARAM_FORMAT"); return; }
    next_limits = *Limits_Get();

    if (strcmp(name, "MAX_CURRENT") == 0)
    {
        float v;
        if (!protocol_parse_float(value, &v)) { protocol_rsp_err("MAX_CURRENT_RANGE"); return; }
        next_limits.max_current_nominal_A = v;
        if (!Limits_IsValid(&next_limits)) { protocol_rsp_err("MAX_CURRENT_RANGE"); return; }
        Limits_Apply(&next_limits);
        state.maxcurrent = next_limits.max_current_peak_A;
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "MAX_CURRENT_PEAK") == 0)
    {
        float v;
        if (!protocol_parse_float(value, &v)) { protocol_rsp_err("MAX_CURRENT_PEAK_RANGE"); return; }
        next_limits.max_current_peak_A = v;
        if (!Limits_IsValid(&next_limits)) { protocol_rsp_err("MAX_CURRENT_PEAK_RANGE"); return; }
        Limits_Apply(&next_limits);
        state.maxcurrent = next_limits.max_current_peak_A;
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "MAX_VELOCITY") == 0)
    {
        float v;
        if (!protocol_parse_float(value, &v)) { protocol_rsp_err("MAX_VELOCITY_RANGE"); return; }
        next_limits.max_velocity_m_s = v;
        if (!Limits_IsValid(&next_limits)) { protocol_rsp_err("MAX_VELOCITY_RANGE"); return; }
        Limits_Apply(&next_limits);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "MAX_ACCELERATION") == 0)
    {
        float v;
        if (!protocol_parse_float(value, &v)) { protocol_rsp_err("MAX_ACCELERATION_RANGE"); return; }
        next_limits.max_acceleration_m_s2 = v;
        if (!Limits_IsValid(&next_limits)) { protocol_rsp_err("MAX_ACCELERATION_RANGE"); return; }
        Limits_Apply(&next_limits);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "SOFT_MIN_POS") == 0)
    {
        int32_t v;
        if (!protocol_parse_i32(value, &v)) { protocol_rsp_err("SOFT_RANGE_INVALID"); return; }
        next_limits.soft_min_pos_um = v;
        if (!Limits_IsValid(&next_limits)) { protocol_rsp_err("SOFT_RANGE_INVALID"); return; }
        Limits_Apply(&next_limits);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "SOFT_MAX_POS") == 0)
    {
        int32_t v;
        if (!protocol_parse_i32(value, &v)) { protocol_rsp_err("SOFT_RANGE_INVALID"); return; }
        next_limits.soft_max_pos_um = v;
        if (!Limits_IsValid(&next_limits)) { protocol_rsp_err("SOFT_RANGE_INVALID"); return; }
        Limits_Apply(&next_limits);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "CALIB_ZERO_POS") == 0)
    {
        int32_t v;
        if (!protocol_parse_i32(value, &v)) { protocol_rsp_err("CALIB_ZERO_RANGE"); return; }
        Calibration_SetZeroPosUm(v);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "CALIB_PITCH_UM") == 0)
    {
        float v;
        if (!protocol_parse_float(value, &v)) { protocol_rsp_err("CALIB_PITCH_RANGE"); return; }
        if (v <= 0.0f) { protocol_rsp_err("CALIB_PITCH_RANGE"); return; }
        Calibration_SetPitchUm(v);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else if (strcmp(name, "CALIB_SIGN") == 0)
    {
        int32_t sign;
        if (!protocol_parse_i32(value, &sign)) { protocol_rsp_err("CALIB_SIGN_RANGE"); return; }
        if ((sign != -1) && (sign != 1)) { protocol_rsp_err("CALIB_SIGN_RANGE"); return; }
        Calibration_SetSign((int8_t)sign);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_saved_ok();
    }
    else protocol_rsp_err("SET_PARAM_UNKNOWN");
}

static void handle_set_config(const char *name, const char *value)
{
    if ((name == NULL) || (value == NULL)) { protocol_rsp_err("SET_CONFIG_FORMAT"); return; }

    if (strcmp(name, "COMMISSION_STAGE") == 0)
    {
        int32_t stage;
        if (!protocol_parse_i32(value, &stage) || (stage < 1) || (stage > 3)) { protocol_rsp_err("STAGE_RANGE"); return; }
        Commissioning_SetStage((uint8_t)stage);
        AxisControl_NotifyConfigChanged();
        protocol_rsp_ok();
    }
    else
    {
        uint8_t v;
        if (!protocol_parse_bool(value, &v)) { protocol_rsp_err("SET_CONFIG_BOOL_RANGE"); return; }

        if (strcmp(name, "SAFE_MODE") == 0) { Commissioning_SetSafeMode(v); AxisControl_NotifyConfigChanged(); protocol_rsp_ok(); }
        else if (strcmp(name, "ARMING_ONLY") == 0) { Commissioning_SetArmingOnly(v); AxisControl_NotifyConfigChanged(); protocol_rsp_ok(); }
        else if (strcmp(name, "CONTROLLED_MOTION") == 0) { Commissioning_SetControlledMotion(v); AxisControl_NotifyConfigChanged(); protocol_rsp_ok(); }
        else if (strcmp(name, "TELEMETRY_ENABLED") == 0) { Telemetry_SetEnabled(v); protocol_rsp_ok(); }
        else if (strcmp(name, "BRAKE_INSTALLED") == 0) { SafetyConfig_SetBrakeInstalled(v); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "COLLISION_SENSOR_INSTALLED") == 0) { SafetyConfig_SetCollisionInstalled(v); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "PTC_INSTALLED") == 0) { SafetyConfig_SetPtcInstalled(v); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "BACKUP_SUPPLY_INSTALLED") == 0) { SafetyConfig_SetBackupSupplyInstalled(v); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "EXTERNAL_INTERLOCK_INSTALLED") == 0) { SafetyConfig_SetExternalInterlockInstalled(v); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "IGNORE_BRAKE_FEEDBACK") == 0) { SafetyConfig_SetIgnoreBrakeFeedback(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 1); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "IGNORE_COLLISION_SENSOR") == 0) { SafetyConfig_SetIgnoreCollisionSensor(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 2); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "IGNORE_EXTERNAL_INTERLOCK") == 0) { SafetyConfig_SetIgnoreExternalInterlock(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 3); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else if (strcmp(name, "ALLOW_MOTION_WITHOUT_CALIBRATION") == 0) { SafetyConfig_SetAllowMotionWithoutCalibration(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 4); AxisControl_NotifyConfigChanged(); protocol_rsp_saved_ok(); }
        else protocol_rsp_err("SET_CONFIG_UNKNOWN");
    }
}

static void handle_cmd(const char *arg1, const char *arg2)
{
    if (arg1 == NULL) { protocol_rsp_err("CMD_FORMAT"); return; }
    if (strcmp(arg1, "HEARTBEAT") == 0) { Watchdogs_KickComm(); protocol_rsp_ok(); }
    else if (strcmp(arg1, "ENABLE") == 0) { if (AxisControl_Enable()) protocol_rsp_ok(); else protocol_rsp_err("ENABLE_REJECTED"); }
    else if (strcmp(arg1, "DISABLE") == 0) { if (AxisControl_Disable()) protocol_rsp_ok(); else protocol_rsp_err("DISABLE_REJECTED"); }
    else if (strcmp(arg1, "STOP") == 0) { if (AxisControl_Stop()) protocol_rsp_ok(); else protocol_rsp_err("STOP_REJECTED"); }
    else if (strcmp(arg1, "QSTOP") == 0) { if (AxisControl_QStop()) protocol_rsp_ok(); else protocol_rsp_err("QSTOP_REJECTED"); }
    else if (strcmp(arg1, "ACK_FAULT") == 0) { Fault_ClearAll(); AxisControl_RefreshState(); protocol_rsp_ok(); }
    else if (strcmp(arg1, "CALIB_CURRENT_ZERO") == 0)
    {
        AxisState_t axis_state = AxisState_Get();
        if (state.enabled || state.HomingOngoing ||
            (axis_state == AXIS_MOTION) || (axis_state == AXIS_CALIBRATION))
        {
            protocol_rsp_err("CALIB_CURRENT_ZERO_NOT_SAFE");
            return;
        }

        MotorState_CalibrateCurrentOffsets((MotorState*)&state);
        SafetyMonitor_Init();
        EventLog_Push(EVT_CALIB_OK, 1);
        protocol_rsp_saved_ok();
    }
    else if (strcmp(arg1, "CALIB_ZERO") == 0)
    {
        if (state.enabled || state.HomingOngoing || (AxisState_Get() == AXIS_MOTION))
        {
            protocol_rsp_err("CALIB_ZERO_NOT_SAFE");
            return;
        }

        AxisState_Set(AXIS_CALIBRATION);
        EventLog_Push(EVT_CALIB_START, 0);

        __disable_irq();
        ResetZeroPosition((MotorState*)&state);
        traj.start_pos_m = state.pos_m;
        traj.dest_pos_m = state.pos_m;
        traj.traj_dist_m = 0.0f;
        traj.T_traj_s = 0.0f;
        traj.a_m_s2 = 0.0f;
        traj.a_set_m_s2 = 0.0f;
        traj.pos_set_m = state.pos_m;
        traj.vel_set_m_s = 0.0f;
        traj.t_elapsed_s = 0.0f;
        traj.traj_done = 1;
        state.pos_set_m = state.pos_m;
        state.vel_set_m_s = 0.0f;
        __enable_irq();

        Calibration_SetZeroPosUm(0);
        Calibration_SetValid(1u);
        state.calibrated = 1u;
        EventLog_Push(EVT_CALIB_OK, (int32_t)state.pos_um);
        AxisControl_NotifyCalibrationComplete(1u);
        protocol_rsp_saved_ok();
    }
    else if (strcmp(arg1, "CALIB_ALIGN_ZERO") == 0)
    {
#if APP_MOTION_IMPLEMENTED
        if (MotionControl_RequestAlignZero()) protocol_rsp_ok();
        else protocol_rsp_err("CALIB_ALIGN_ZERO_REJECTED");
#else
        protocol_rsp_err("CALIB_ALIGN_ZERO_UNAVAILABLE");
#endif
    }
    else if (strcmp(arg1, "HOME") == 0)
    {
#if APP_MOTION_IMPLEMENTED
        if (MotionControl_RequestHoming()) protocol_rsp_ok();
        else protocol_rsp_err("HOME_REJECTED");
#else
        protocol_rsp_err("HOME_UNAVAILABLE");
#endif
    }
    else if (strcmp(arg1, "MOVE_REL") == 0)
    {
        int32_t delta_um;
        if (arg2 == NULL) { protocol_rsp_err("MOVE_REL_FORMAT"); return; }
        if (!protocol_parse_i32(arg2, &delta_um)) { protocol_rsp_err("MOVE_REL_RANGE"); return; }
        if (AxisControl_MoveRelUm(delta_um)) protocol_rsp_ok();
        else protocol_rsp_err("MOVE_REL_REJECTED");
    }
    else if (strcmp(arg1, "FIRST_MOVE_REL") == 0)
    {
        int32_t delta_um;
        if (arg2 == NULL) { protocol_rsp_err("FIRST_MOVE_REL_FORMAT"); return; }
        if (!protocol_parse_i32(arg2, &delta_um)) { protocol_rsp_err("FIRST_MOVE_REL_RANGE"); return; }
        if (AxisControl_FirstMoveRelUm(delta_um)) protocol_rsp_ok();
        else protocol_rsp_err("FIRST_MOVE_REL_REJECTED");
    }
    else if (strcmp(arg1, "MOVE_ABS") == 0)
    {
        int32_t target_um;
        if (arg2 == NULL) { protocol_rsp_err("MOVE_ABS_FORMAT"); return; }
        if (!protocol_parse_i32(arg2, &target_um)) { protocol_rsp_err("MOVE_ABS_RANGE"); return; }
        if (AxisControl_MoveAbsUm(target_um)) protocol_rsp_ok();
        else protocol_rsp_err("MOVE_ABS_REJECTED");
    }
    else protocol_rsp_err("CMD_UNKNOWN");
}

void Protocol_Process(void)
{
    uint8_t pending_errors;
    char local_buf[PROTOCOL_RX_BUFFER_SIZE];
    char *cmd, *arg1, *arg2, *arg3;

    __disable_irq();
    pending_errors = g_pending_errors;
    g_pending_errors = 0u;
    __enable_irq();

    if ((pending_errors & PROTOCOL_PENDING_ERR_RX_OVERFLOW) != 0u)
        protocol_rsp_err("RX_OVERFLOW");
    if ((pending_errors & PROTOCOL_PENDING_ERR_CMD_OVERFLOW) != 0u)
        protocol_rsp_err("CMD_OVERFLOW");

    Protocol_EnsureRxActive();

    __disable_irq();
    if (g_cmd_count == 0u)
    {
        __enable_irq();
        return;
    }

    memset(local_buf, 0, sizeof(local_buf));
    strncpy(local_buf, g_cmd_queue[g_cmd_tail], sizeof(local_buf) - 1u);
    g_cmd_tail = (uint8_t)((g_cmd_tail + 1u) % PROTOCOL_CMD_QUEUE_DEPTH);
    g_cmd_count--;
    __enable_irq();

    cmd = strtok(local_buf, " ");
    arg1 = strtok(NULL, " ");
    arg2 = strtok(NULL, " ");
    arg3 = strtok(NULL, " ");

    if (cmd == NULL) { protocol_rsp_err("EMPTY"); return; }
    Watchdogs_KickComm();

    if (strcmp(cmd, "GET") == 0)
    {
        if ((arg1 != NULL) && (strcmp(arg1, "PARAM") == 0)) handle_get_param(arg2);
        else if ((arg1 != NULL) && (strcmp(arg1, "CONFIG") == 0)) handle_get_config(arg2);
        else handle_get_simple(arg1);
    }
    else if (strcmp(cmd, "SET") == 0)
    {
        if ((arg1 != NULL) && (strcmp(arg1, "PARAM") == 0)) handle_set_param(arg2, arg3);
        else if ((arg1 != NULL) && (strcmp(arg1, "CONFIG") == 0)) handle_set_config(arg2, arg3);
        else protocol_rsp_err("SET_UNKNOWN");
    }
    else if (strcmp(cmd, "CMD") == 0) handle_cmd(arg1, arg2);
    else protocol_rsp_err("UNKNOWN");
}
