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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROTOCOL_RX_BUFFER_SIZE 128u

extern volatile MotorState state;

static uint8_t g_rx_byte = 0u;
static char g_rx_buffer[PROTOCOL_RX_BUFFER_SIZE];
static uint16_t g_rx_index = 0u;
static uint8_t g_cmd_ready = 0u;

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

void Protocol_Init(void)
{
    g_rx_index = 0u;
    g_cmd_ready = 0u;
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1);
}

void Protocol_RxCpltCallback(uint8_t byte)
{
    if ((byte == '\n') || (byte == '\r'))
    {
        if (g_rx_index > 0u)
        {
            g_rx_buffer[g_rx_index] = '\0';
            g_cmd_ready = 1u;
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
            protocol_rsp_err("RX_OVERFLOW");
        }
    }
    HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1);
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
    else if (strcmp(arg1, "POS") == 0)
        protocol_rsp_param_i32("POS", (int32_t)state.pos_um);
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
    if ((name == NULL) || (value == NULL)) { protocol_rsp_err("SET_PARAM_FORMAT"); return; }
    if (strcmp(name, "MAX_CURRENT") == 0)
    {
        float v = (float)atof(value);
        if ((v < 0.0f) || (v > 1.0f)) { protocol_rsp_err("MAX_CURRENT_RANGE"); return; }
        Limits_SetMaxCurrentNominal(v);
        state.maxcurrent = v;
        protocol_rsp_ok();
    }
    else if (strcmp(name, "MAX_CURRENT_PEAK") == 0)
    {
        float v = (float)atof(value);
        if ((v < 0.0f) || (v > 2.0f)) { protocol_rsp_err("MAX_CURRENT_PEAK_RANGE"); return; }
        Limits_SetMaxCurrentPeak(v);
        protocol_rsp_ok();
    }
    else if (strcmp(name, "MAX_VELOCITY") == 0)
    {
        float v = (float)atof(value);
        if ((v < 0.0f) || (v > 0.1f)) { protocol_rsp_err("MAX_VELOCITY_RANGE"); return; }
        Limits_SetMaxVelocity(v);
        protocol_rsp_ok();
    }
    else if (strcmp(name, "MAX_ACCELERATION") == 0)
    {
        float v = (float)atof(value);
        if ((v < 0.0f) || (v > 1.0f)) { protocol_rsp_err("MAX_ACCELERATION_RANGE"); return; }
        Limits_SetMaxAcceleration(v);
        protocol_rsp_ok();
    }
    else if (strcmp(name, "SOFT_MIN_POS") == 0)
    {
        Limits_SetSoftMinPos((int32_t)atol(value));
        protocol_rsp_ok();
    }
    else if (strcmp(name, "SOFT_MAX_POS") == 0)
    {
        Limits_SetSoftMaxPos((int32_t)atol(value));
        protocol_rsp_ok();
    }
    else if (strcmp(name, "CALIB_ZERO_POS") == 0)
    {
        Calibration_SetZeroPosUm((int32_t)atol(value));
        protocol_rsp_ok();
    }
    else if (strcmp(name, "CALIB_PITCH_UM") == 0)
    {
        float v = (float)atof(value);
        if (v <= 0.0f) { protocol_rsp_err("CALIB_PITCH_RANGE"); return; }
        Calibration_SetPitchUm(v);
        protocol_rsp_ok();
    }
    else if (strcmp(name, "CALIB_SIGN") == 0)
    {
        int32_t sign = atol(value);
        if ((sign != -1) && (sign != 1)) { protocol_rsp_err("CALIB_SIGN_RANGE"); return; }
        Calibration_SetSign((int8_t)sign);
        protocol_rsp_ok();
    }
    else protocol_rsp_err("SET_PARAM_UNKNOWN");
}

static void handle_set_config(const char *name, const char *value)
{
    uint8_t v;
    if ((name == NULL) || (value == NULL)) { protocol_rsp_err("SET_CONFIG_FORMAT"); return; }
    v = (uint8_t)atoi(value);

    if (strcmp(name, "SAFE_MODE") == 0) { Commissioning_SetSafeMode(v); protocol_rsp_ok(); }
    else if (strcmp(name, "ARMING_ONLY") == 0) { Commissioning_SetArmingOnly(v); protocol_rsp_ok(); }
    else if (strcmp(name, "CONTROLLED_MOTION") == 0) { Commissioning_SetControlledMotion(v); protocol_rsp_ok(); }
    else if (strcmp(name, "COMMISSION_STAGE") == 0)
    {
        uint8_t stage = (uint8_t)atoi(value);
        if ((stage < 1u) || (stage > 3u)) { protocol_rsp_err("STAGE_RANGE"); return; }
        Commissioning_SetStage(stage);
        protocol_rsp_ok();
    }
    else if (strcmp(name, "BRAKE_INSTALLED") == 0) { SafetyConfig_SetBrakeInstalled(v); protocol_rsp_ok(); }
    else if (strcmp(name, "COLLISION_SENSOR_INSTALLED") == 0) { SafetyConfig_SetCollisionInstalled(v); protocol_rsp_ok(); }
    else if (strcmp(name, "PTC_INSTALLED") == 0) { SafetyConfig_SetPtcInstalled(v); protocol_rsp_ok(); }
    else if (strcmp(name, "BACKUP_SUPPLY_INSTALLED") == 0) { SafetyConfig_SetBackupSupplyInstalled(v); protocol_rsp_ok(); }
    else if (strcmp(name, "EXTERNAL_INTERLOCK_INSTALLED") == 0) { SafetyConfig_SetExternalInterlockInstalled(v); protocol_rsp_ok(); }
    else if (strcmp(name, "IGNORE_BRAKE_FEEDBACK") == 0) { SafetyConfig_SetIgnoreBrakeFeedback(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 1); protocol_rsp_ok(); }
    else if (strcmp(name, "IGNORE_COLLISION_SENSOR") == 0) { SafetyConfig_SetIgnoreCollisionSensor(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 2); protocol_rsp_ok(); }
    else if (strcmp(name, "IGNORE_EXTERNAL_INTERLOCK") == 0) { SafetyConfig_SetIgnoreExternalInterlock(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 3); protocol_rsp_ok(); }
    else if (strcmp(name, "ALLOW_MOTION_WITHOUT_CALIBRATION") == 0) { SafetyConfig_SetAllowMotionWithoutCalibration(v); EventLog_Push(v ? EVT_OVERRIDE_ON : EVT_OVERRIDE_OFF, 4); protocol_rsp_ok(); }
    else protocol_rsp_err("SET_CONFIG_UNKNOWN");
}

static void handle_cmd(const char *arg1, const char *arg2)
{
    if (arg1 == NULL) { protocol_rsp_err("CMD_FORMAT"); return; }
    if (strcmp(arg1, "HEARTBEAT") == 0) { Watchdogs_KickComm(); protocol_rsp_ok(); }
    else if (strcmp(arg1, "ENABLE") == 0) { if (AxisControl_Enable()) protocol_rsp_ok(); else protocol_rsp_err("ENABLE_REJECTED"); }
    else if (strcmp(arg1, "DISABLE") == 0) { if (AxisControl_Disable()) protocol_rsp_ok(); else protocol_rsp_err("DISABLE_REJECTED"); }
    else if (strcmp(arg1, "STOP") == 0) { if (AxisControl_Stop()) protocol_rsp_ok(); else protocol_rsp_err("STOP_REJECTED"); }
    else if (strcmp(arg1, "QSTOP") == 0) { if (AxisControl_QStop()) protocol_rsp_ok(); else protocol_rsp_err("QSTOP_REJECTED"); }
    else if (strcmp(arg1, "ACK_FAULT") == 0) { Fault_ClearAll(); AxisState_Set(AXIS_SAFE); protocol_rsp_ok(); }
    else if (strcmp(arg1, "CALIB_ZERO") == 0)
    {
        AxisState_Set(AXIS_CALIBRATION);
        EventLog_Push(EVT_CALIB_START, 0);
        Calibration_SetZeroPosUm((int32_t)state.pos_um);
        Calibration_SetValid(1u);
        state.calibrated = 1u;
        EventLog_Push(EVT_CALIB_OK, (int32_t)state.pos_um);
        AxisState_Set(AXIS_SAFE);
        protocol_rsp_ok();
    }
    else if (strcmp(arg1, "MOVE_REL") == 0)
    {
        if (arg2 == NULL) { protocol_rsp_err("MOVE_REL_FORMAT"); return; }
        if (AxisControl_MoveRelUm((int32_t)atol(arg2))) protocol_rsp_ok();
        else protocol_rsp_err("MOVE_REL_REJECTED");
    }
    else if (strcmp(arg1, "MOVE_ABS") == 0)
    {
        if (arg2 == NULL) { protocol_rsp_err("MOVE_ABS_FORMAT"); return; }
        if (AxisControl_MoveAbsUm((int32_t)atol(arg2))) protocol_rsp_ok();
        else protocol_rsp_err("MOVE_ABS_REJECTED");
    }
    else protocol_rsp_err("CMD_UNKNOWN");
}

void Protocol_Process(void)
{
    if (!g_cmd_ready) return;
    g_cmd_ready = 0u;

    char local_buf[PROTOCOL_RX_BUFFER_SIZE];
    char *cmd, *arg1, *arg2, *arg3;

    memset(local_buf, 0, sizeof(local_buf));
    strncpy(local_buf, g_rx_buffer, sizeof(local_buf) - 1u);

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
