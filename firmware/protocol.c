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
#define PROTOCOL_BIN_QUEUE_DEPTH 4u
#define PROTOCOL_V2_SOF1 0xAAu
#define PROTOCOL_V2_SOF2 0x55u
#define PROTOCOL_V2_MAX_PAYLOAD 64u
#define PROTOCOL_V2_MAX_FRAME (2u + 1u + 1u + 2u + PROTOCOL_V2_MAX_PAYLOAD + 2u)

#define PROTOCOL_PENDING_ERR_RX_OVERFLOW (1u << 0)
#define PROTOCOL_PENDING_ERR_CMD_OVERFLOW (1u << 1)
#define PROTOCOL_PENDING_ERR_BIN_OVERFLOW (1u << 2)
#define PROTOCOL_PENDING_ERR_BIN_CRC (1u << 3)
#define PROTOCOL_PENDING_ERR_BIN_LEN (1u << 4)

#define PROTOCOL_V2_STREAM_RUNTIME_STATUS 0x02u
#define PROTOCOL_V2_STREAM_LIMITS 0x03u
#define PROTOCOL_V2_STREAM_SAFETY_CONFIG 0x04u
#define PROTOCOL_V2_STREAM_CALIBRATION 0x05u
#define PROTOCOL_V2_STREAM_VERSION 0x07u

#define PROTOCOL_V2_REQUEST_RUNTIME_STATUS (1u << 0)
#define PROTOCOL_V2_REQUEST_LIMITS (1u << 1)
#define PROTOCOL_V2_REQUEST_SAFETY_CONFIG (1u << 2)
#define PROTOCOL_V2_REQUEST_CALIBRATION (1u << 3)
#define PROTOCOL_V2_REQUEST_VERSION (1u << 4)

#define PROTOCOL_V2_VERSION_MAJOR 0x02u
#define PROTOCOL_V2_VERSION_MINOR 0x00u

#define PROTOCOL_V2_CAP_FAST_MOTION      (1u << 0)
#define PROTOCOL_V2_CAP_RUNTIME_STATUS   (1u << 1)
#define PROTOCOL_V2_CAP_LIMITS           (1u << 2)
#define PROTOCOL_V2_CAP_SAFETY_CONFIG    (1u << 3)
#define PROTOCOL_V2_CAP_CALIBRATION      (1u << 4)
#define PROTOCOL_V2_CAP_VERSION          (1u << 5)
#define PROTOCOL_V2_CAP_BINARY_COMMANDS  (1u << 6)
#define PROTOCOL_V2_CAP_HEARTBEAT        (1u << 7)

#define PROTOCOL_V2_CMD_MOVE_RELATIVE 0x02u
#define PROTOCOL_V2_CMD_MOVE_ABSOLUTE 0x03u
#define PROTOCOL_V2_CMD_STOP 0x04u
#define PROTOCOL_V2_CMD_RESET_FAULT 0x05u
#define PROTOCOL_V2_CMD_REQUEST_STATUS 0x06u
#define PROTOCOL_V2_CMD_ENABLE 0x07u
#define PROTOCOL_V2_CMD_DISABLE 0x08u
#define PROTOCOL_V2_CMD_CALIBRATION_ZERO 0x0Bu

#define PROTOCOL_V2_RSP_ACK 0x01u
#define PROTOCOL_V2_RSP_NACK 0x02u
#define PROTOCOL_V2_RSP_SNAPSHOT 0x03u

#define PROTOCOL_V2_ERR_CRC_FAIL 0x01u
#define PROTOCOL_V2_ERR_LEN_FAIL 0x02u
#define PROTOCOL_V2_ERR_UNKNOWN_TYPE 0x03u
#define PROTOCOL_V2_ERR_UNKNOWN_COMMAND 0x04u
#define PROTOCOL_V2_ERR_STATE_REJECTED 0x06u
#define PROTOCOL_V2_ERR_BUSY 0x08u

#define PROTOCOL_V2_HEARTBEAT_PC_TO_MCU 0x01u
#define PROTOCOL_V2_HEARTBEAT_MCU_TO_PC 0x02u

extern volatile MotorState state;
extern volatile MotorTrajectory traj;

static uint8_t g_rx_byte = 0u;
static char g_rx_buffer[PROTOCOL_RX_BUFFER_SIZE];
static char g_cmd_queue[PROTOCOL_CMD_QUEUE_DEPTH][PROTOCOL_RX_BUFFER_SIZE];
typedef struct {
    uint8_t type;
    uint16_t seq;
    uint8_t payload[PROTOCOL_V2_MAX_PAYLOAD];
    uint8_t payload_len;
} ProtocolBinaryFrame_t;
static ProtocolBinaryFrame_t g_bin_queue[PROTOCOL_BIN_QUEUE_DEPTH];
static uint16_t g_rx_index = 0u;
static volatile uint8_t g_cmd_head = 0u;
static volatile uint8_t g_cmd_tail = 0u;
static volatile uint8_t g_cmd_count = 0u;
static volatile uint8_t g_bin_head = 0u;
static volatile uint8_t g_bin_tail = 0u;
static volatile uint8_t g_bin_count = 0u;
static volatile uint8_t g_pending_errors = 0u;
static volatile uint8_t g_rx_active = 0u;
static uint16_t g_tx_seq = 0u;

typedef enum {
    PROTOCOL_RX_MODE_ASCII = 0,
    PROTOCOL_RX_MODE_BIN_WAIT_SOF2,
    PROTOCOL_RX_MODE_BIN_LEN,
    PROTOCOL_RX_MODE_BIN_TYPE,
    PROTOCOL_RX_MODE_BIN_SEQ0,
    PROTOCOL_RX_MODE_BIN_SEQ1,
    PROTOCOL_RX_MODE_BIN_PAYLOAD,
    PROTOCOL_RX_MODE_BIN_CRC0,
    PROTOCOL_RX_MODE_BIN_CRC1
} ProtocolRxMode_t;

static ProtocolRxMode_t g_rx_mode = PROTOCOL_RX_MODE_ASCII;
static uint8_t g_bin_len = 0u;
static uint8_t g_bin_type = 0u;
static uint16_t g_bin_seq = 0u;
static uint8_t g_bin_payload[PROTOCOL_V2_MAX_PAYLOAD];
static uint8_t g_bin_payload_len = 0u;
static uint8_t g_bin_payload_index = 0u;
static uint8_t g_bin_crc_lo = 0u;

uint8_t Protocol_GetLastRxByte(void){ return g_rx_byte; }

void Protocol_SendLine(const char *line)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)line, (uint16_t)strlen(line), 100);
}

uint8_t Protocol_BinaryV2Enabled(void)
{
#if APP_PROTOCOL_BINARY_V2
    return 1u;
#else
    return 0u;
#endif
}

static uint16_t protocol_crc16_ccitt_false(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t bit;

    if (data == 0)
        return crc;

    for (i = 0u; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            else
                crc <<= 1;
        }
    }

    return crc;
}

static void protocol_put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void protocol_put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void protocol_put_i32_le(uint8_t *dst, int32_t value)
{
    protocol_put_u32_le(dst, (uint32_t)value);
}

static uint16_t protocol_read_u16_le(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static int32_t protocol_read_i32_le(const uint8_t *src)
{
    return (int32_t)((uint32_t)src[0]
                   | ((uint32_t)src[1] << 8)
                   | ((uint32_t)src[2] << 16)
                   | ((uint32_t)src[3] << 24));
}

static uint32_t protocol_hash_string32(const char *text)
{
    uint32_t hash = 2166136261u;

    if (text == 0)
        return 0u;

    while (*text != '\0')
    {
        hash ^= (uint8_t)(*text);
        hash *= 16777619u;
        ++text;
    }

    return hash;
}

static void protocol_parse_fw_version(uint8_t *major_out, uint8_t *minor_out, uint16_t *patch_out)
{
    unsigned major = 0u;
    unsigned minor = 0u;
    unsigned patch = 0u;

    (void)sscanf(FwVersion_Version(), "%u.%u.%u", &major, &minor, &patch);

    if (major_out != 0) *major_out = (uint8_t)(major & 0xFFu);
    if (minor_out != 0) *minor_out = (uint8_t)(minor & 0xFFu);
    if (patch_out != 0) *patch_out = (uint16_t)(patch & 0xFFFFu);
}

static void protocol_queue_binary_frame(void)
{
    ProtocolBinaryFrame_t *slot;

    if (g_bin_count >= PROTOCOL_BIN_QUEUE_DEPTH)
    {
        protocol_queue_error(PROTOCOL_PENDING_ERR_BIN_OVERFLOW);
        return;
    }

    slot = &g_bin_queue[g_bin_head];
    slot->type = g_bin_type;
    slot->seq = g_bin_seq;
    slot->payload_len = g_bin_payload_len;
    if (g_bin_payload_len > 0u)
        memcpy(slot->payload, g_bin_payload, g_bin_payload_len);
    g_bin_head = (uint8_t)((g_bin_head + 1u) % PROTOCOL_BIN_QUEUE_DEPTH);
    g_bin_count++;
}

static void protocol_reset_binary_rx_state(void)
{
    g_rx_mode = PROTOCOL_RX_MODE_ASCII;
    g_bin_len = 0u;
    g_bin_type = 0u;
    g_bin_seq = 0u;
    g_bin_payload_len = 0u;
    g_bin_payload_index = 0u;
    g_bin_crc_lo = 0u;
}

void Protocol_SendFrame(uint8_t type, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t frame[PROTOCOL_V2_MAX_FRAME];
    uint8_t len_field;
    uint16_t crc;
    uint16_t seq;
    uint16_t crc_input_len;
    uint16_t tx_len;

    if ((payload_len > PROTOCOL_V2_MAX_PAYLOAD) || ((payload_len > 0u) && (payload == 0)))
        return;

    len_field = (uint8_t)(1u + 2u + payload_len);
    seq = g_tx_seq++;

    frame[0] = PROTOCOL_V2_SOF1;
    frame[1] = PROTOCOL_V2_SOF2;
    frame[2] = len_field;
    frame[3] = type;
    frame[4] = (uint8_t)(seq & 0xFFu);
    frame[5] = (uint8_t)((seq >> 8) & 0xFFu);
    if (payload_len > 0u)
        memcpy(&frame[6], payload, payload_len);

    crc_input_len = (uint16_t)(1u + len_field);
    crc = protocol_crc16_ccitt_false(&frame[2], crc_input_len);
    frame[6u + payload_len] = (uint8_t)(crc & 0xFFu);
    frame[7u + payload_len] = (uint8_t)((crc >> 8) & 0xFFu);
    tx_len = (uint16_t)(8u + payload_len);

    HAL_UART_Transmit(&huart2, frame, tx_len, 100);
}

static void protocol_rsp_v2(uint8_t response_id,
                            uint16_t ack_seq,
                            uint16_t status_code,
                            const uint8_t *body,
                            uint8_t body_len)
{
    uint8_t payload[PROTOCOL_V2_MAX_PAYLOAD];

    if ((body_len + 5u) > sizeof(payload))
        return;

    payload[0] = response_id;
    protocol_put_u16_le(&payload[1], ack_seq);
    protocol_put_u16_le(&payload[3], status_code);
    if ((body_len > 0u) && (body != 0))
        memcpy(&payload[5], body, body_len);
    Protocol_SendFrame(PROTOCOL_FRAME_RESPONSE, payload, (uint8_t)(5u + body_len));
}

static void protocol_rsp_v2_ack(uint16_t ack_seq)
{
    protocol_rsp_v2(PROTOCOL_V2_RSP_ACK, ack_seq, 0u, 0, 0u);
}

static void protocol_rsp_v2_nack(uint16_t ack_seq, uint16_t status_code)
{
    protocol_rsp_v2(PROTOCOL_V2_RSP_NACK, ack_seq, status_code, 0, 0u);
}

static void protocol_err_v2(uint8_t error_code, uint16_t ref_seq, uint16_t detail_code)
{
    uint8_t payload[10];

    payload[0] = error_code;
    protocol_put_u16_le(&payload[1], ref_seq);
    protocol_put_u16_le(&payload[3], detail_code);
    protocol_put_u32_le(&payload[5], Fault_GetMask());
    payload[9] = (uint8_t)AxisState_Get();
    Protocol_SendFrame(PROTOCOL_FRAME_ERROR, payload, (uint8_t)sizeof(payload));
}

uint16_t Protocol_BuildStatusFlags(void)
{
    uint16_t flags = 0u;

    if (state.enabled) flags |= (uint16_t)(1u << 0);
    if (AxisControl_RunAllowed()) flags |= (uint16_t)(1u << 1);
    if (Commissioning_SafeMode()) flags |= (uint16_t)(1u << 2);
    if (Commissioning_ArmingOnly()) flags |= (uint16_t)(1u << 3);
    if (Commissioning_ControlledMotion()) flags |= (uint16_t)(1u << 4);
    if (Telemetry_IsEnabled()) flags |= (uint16_t)(1u << 5);
    if (ConfigStore_IsLoaded()) flags |= (uint16_t)(1u << 6);
    if (state.Vbus_valid) flags |= (uint16_t)(1u << 7);
    if (state.calibrated) flags |= (uint16_t)(1u << 8);
    if (state.HomingStarted) flags |= (uint16_t)(1u << 9);
    if (state.HomingOngoing) flags |= (uint16_t)(1u << 10);
    if (state.HomingSuccessful) flags |= (uint16_t)(1u << 11);
    if (AxisControl_FirstMoveTestActive()) flags |= (uint16_t)(1u << 12);
#if APP_SAFE_INTEGRATION
    flags |= (uint16_t)(1u << 13);
#endif
#if APP_MOTION_IMPLEMENTED
    flags |= (uint16_t)(1u << 14);
#endif
    if (SafetyMonitor_PowerReady((const volatile MotorState*)&state)) flags |= (uint16_t)(1u << 15);

    return flags;
}

uint16_t Protocol_BuildErrorFlags(void)
{
    uint16_t flags = 0u;
    uint32_t mask = Fault_GetMask();

    if ((mask & (1u << (FAULT_COMM_TIMEOUT - 1))) != 0u) flags |= (uint16_t)(1u << 0);
    if ((mask & (1u << (FAULT_OVERCURRENT - 1))) != 0u) flags |= (uint16_t)(1u << 1);
    if ((mask & (1u << (FAULT_UNDERVOLTAGE - 1))) != 0u) flags |= (uint16_t)(1u << 2);
    if ((mask & (1u << (FAULT_OVERVOLTAGE - 1))) != 0u) flags |= (uint16_t)(1u << 3);
    if ((mask & (1u << (FAULT_OVERSPEED - 1))) != 0u) flags |= (uint16_t)(1u << 4);
    if ((mask & (1u << (FAULT_TRACKING - 1))) != 0u) flags |= (uint16_t)(1u << 5);
    if ((mask & (1u << (FAULT_ESTOP - 1))) != 0u) flags |= (uint16_t)(1u << 6);
    if ((mask & (1u << (FAULT_CONFIG_INVALID - 1))) != 0u) flags |= (uint16_t)(1u << 7);
    if ((mask & (1u << (FAULT_NOT_CALIBRATED - 1))) != 0u) flags |= (uint16_t)(1u << 8);
    if ((mask & (1u << (FAULT_ENCODER_INVALID - 1))) != 0u) flags |= (uint16_t)(1u << 9);
    if ((mask & (1u << (FAULT_OVERTEMP - 1))) != 0u) flags |= (uint16_t)(1u << 11);

    return flags;
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
    g_bin_head = 0u;
    g_bin_tail = 0u;
    g_bin_count = 0u;
    g_pending_errors = 0u;
    g_rx_active = 0u;
    g_tx_seq = 0u;
    protocol_reset_binary_rx_state();
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    memset(g_cmd_queue, 0, sizeof(g_cmd_queue));
    memset(g_bin_queue, 0, sizeof(g_bin_queue));
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
    protocol_reset_binary_rx_state();
    __HAL_UART_CLEAR_OREFLAG(&huart2);
    (void)HAL_UART_AbortReceive_IT(&huart2);
    Protocol_EnsureRxActive();
}

void Protocol_RxCpltCallback(uint8_t byte)
{
    uint8_t crc_buf[1u + 1u + 2u + PROTOCOL_V2_MAX_PAYLOAD];
    uint16_t crc_calc;
    uint16_t crc_rx;

    g_rx_active = 0u;
    if (g_rx_mode != PROTOCOL_RX_MODE_ASCII)
    {
        switch (g_rx_mode)
        {
        case PROTOCOL_RX_MODE_BIN_WAIT_SOF2:
            if (byte == PROTOCOL_V2_SOF2)
                g_rx_mode = PROTOCOL_RX_MODE_BIN_LEN;
            else if (byte != PROTOCOL_V2_SOF1)
                protocol_reset_binary_rx_state();
            break;
        case PROTOCOL_RX_MODE_BIN_LEN:
            g_bin_len = byte;
            if ((g_bin_len < 3u) || (g_bin_len > (uint8_t)(1u + 2u + PROTOCOL_V2_MAX_PAYLOAD)))
            {
                protocol_queue_error(PROTOCOL_PENDING_ERR_BIN_LEN);
                protocol_reset_binary_rx_state();
            }
            else
            {
                g_bin_payload_len = (uint8_t)(g_bin_len - 3u);
                g_rx_mode = PROTOCOL_RX_MODE_BIN_TYPE;
            }
            break;
        case PROTOCOL_RX_MODE_BIN_TYPE:
            g_bin_type = byte;
            g_rx_mode = PROTOCOL_RX_MODE_BIN_SEQ0;
            break;
        case PROTOCOL_RX_MODE_BIN_SEQ0:
            g_bin_seq = byte;
            g_rx_mode = PROTOCOL_RX_MODE_BIN_SEQ1;
            break;
        case PROTOCOL_RX_MODE_BIN_SEQ1:
            g_bin_seq |= (uint16_t)byte << 8;
            g_bin_payload_index = 0u;
            g_rx_mode = (g_bin_payload_len > 0u) ? PROTOCOL_RX_MODE_BIN_PAYLOAD : PROTOCOL_RX_MODE_BIN_CRC0;
            break;
        case PROTOCOL_RX_MODE_BIN_PAYLOAD:
            g_bin_payload[g_bin_payload_index++] = byte;
            if (g_bin_payload_index >= g_bin_payload_len)
                g_rx_mode = PROTOCOL_RX_MODE_BIN_CRC0;
            break;
        case PROTOCOL_RX_MODE_BIN_CRC0:
            g_bin_crc_lo = byte;
            g_rx_mode = PROTOCOL_RX_MODE_BIN_CRC1;
            break;
        case PROTOCOL_RX_MODE_BIN_CRC1:
            crc_rx = (uint16_t)g_bin_crc_lo | ((uint16_t)byte << 8);
            crc_buf[0] = g_bin_len;
            crc_buf[1] = g_bin_type;
            crc_buf[2] = (uint8_t)(g_bin_seq & 0xFFu);
            crc_buf[3] = (uint8_t)((g_bin_seq >> 8) & 0xFFu);
            if (g_bin_payload_len > 0u)
                memcpy(&crc_buf[4], g_bin_payload, g_bin_payload_len);
            crc_calc = protocol_crc16_ccitt_false(crc_buf, (uint16_t)(4u + g_bin_payload_len));
            if (crc_calc == crc_rx)
                protocol_queue_binary_frame();
            else
                protocol_queue_error(PROTOCOL_PENDING_ERR_BIN_CRC);
            protocol_reset_binary_rx_state();
            break;
        default:
            protocol_reset_binary_rx_state();
            break;
        }
    }
    else
    {
        if (byte == PROTOCOL_V2_SOF1)
        {
            g_rx_index = 0u;
            g_rx_mode = PROTOCOL_RX_MODE_BIN_WAIT_SOF2;
        }
        else if ((byte == '\n') || (byte == '\r'))
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
    else if (strcmp(arg1, "PROTOCOL_BINARY_V2") == 0)
        protocol_rsp_config_u8("PROTOCOL_BINARY_V2", APP_PROTOCOL_BINARY_V2 ? 1u : 0u);
    else if (strcmp(arg1, "PROTOCOL_BINARY_V2_TELEMETRY") == 0)
        protocol_rsp_config_u8("PROTOCOL_BINARY_V2_TELEMETRY", APP_PROTOCOL_BINARY_V2_TELEMETRY ? 1u : 0u);
    else if (strcmp(arg1, "DIAG_ASSUME_COMMUNICATION_OK") == 0)
        protocol_rsp_config_u8("DIAG_ASSUME_COMMUNICATION_OK", APP_DIAG_ASSUME_COMMUNICATION_OK ? 1u : 0u);
    else if (strcmp(arg1, "DIAG_ASSUME_COMMAND_ACCEPTANCE_OK") == 0)
        protocol_rsp_config_u8("DIAG_ASSUME_COMMAND_ACCEPTANCE_OK", APP_DIAG_ASSUME_COMMAND_ACCEPTANCE_OK ? 1u : 0u);
    else if (strcmp(arg1, "DIAG_EXECUTION_FOCUS") == 0)
        protocol_rsp_config_u8("DIAG_EXECUTION_FOCUS", APP_DIAG_EXECUTION_FOCUS ? 1u : 0u);
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

static void protocol_send_runtime_snapshot_v2(uint16_t ack_seq)
{
    uint8_t body[18];

    body[0] = PROTOCOL_V2_STREAM_RUNTIME_STATUS;
    protocol_put_u32_le(&body[1], HAL_GetTick() * 1000u);
    body[5] = (uint8_t)Fault_GetLast();
    protocol_put_u32_le(&body[6], Fault_GetMask());
    body[10] = (uint8_t)AxisState_Get();
    body[11] = Commissioning_GetStage();
    protocol_put_u16_le(&body[12], Protocol_BuildStatusFlags());
    protocol_put_u16_le(&body[14], Protocol_BuildErrorFlags());
    protocol_put_u16_le(&body[16], (uint16_t)(state.Vbus > 0.0f ? (state.Vbus * 1000.0f) : 0.0f));

    protocol_rsp_v2(PROTOCOL_V2_RSP_SNAPSHOT, ack_seq, 0u, body, (uint8_t)sizeof(body));
}

static uint16_t protocol_build_install_flags(void)
{
    const SafetyConfig_t *cfg = SafetyConfig_Get();
    uint16_t flags = 0u;

    if (cfg->brake_installed) flags |= (uint16_t)(1u << 0);
    if (cfg->collision_sensor_installed) flags |= (uint16_t)(1u << 1);
    if (cfg->ptc_installed) flags |= (uint16_t)(1u << 2);
    if (cfg->backup_supply_installed) flags |= (uint16_t)(1u << 3);
    if (cfg->external_interlock_installed) flags |= (uint16_t)(1u << 4);

    return flags;
}

static uint16_t protocol_build_override_flags(void)
{
    const SafetyConfig_t *cfg = SafetyConfig_Get();
    uint16_t flags = 0u;

    if (cfg->ignore_brake_feedback) flags |= (uint16_t)(1u << 0);
    if (cfg->ignore_collision_sensor) flags |= (uint16_t)(1u << 1);
    if (cfg->ignore_external_interlock) flags |= (uint16_t)(1u << 2);

    return flags;
}

static uint16_t protocol_build_behavior_flags(void)
{
    uint16_t flags = 0u;
    const SafetyConfig_t *cfg = SafetyConfig_Get();

    if (cfg->allow_motion_without_calibration) flags |= (uint16_t)(1u << 0);
    if (Telemetry_IsEnabled()) flags |= (uint16_t)(1u << 1);
    if (ConfigStore_IsLoaded()) flags |= (uint16_t)(1u << 2);
#if APP_SAFE_INTEGRATION
    flags |= (uint16_t)(1u << 3);
#endif
#if APP_MOTION_IMPLEMENTED
    flags |= (uint16_t)(1u << 4);
#endif

    return flags;
}

static void protocol_send_limits_snapshot_v2(uint16_t ack_seq)
{
    uint8_t body[25];
    const Limits_t *limits = Limits_Get();

    body[0] = PROTOCOL_V2_STREAM_LIMITS;
    protocol_put_u32_le(&body[1], HAL_GetTick() * 1000u);
    protocol_put_u16_le(&body[5], (uint16_t)(limits->max_current_nominal_A > 0.0f ? (limits->max_current_nominal_A * 1000.0f) : 0.0f));
    protocol_put_u16_le(&body[7], (uint16_t)(limits->max_current_peak_A > 0.0f ? (limits->max_current_peak_A * 1000.0f) : 0.0f));
    protocol_put_u32_le(&body[9], (uint32_t)(limits->max_velocity_m_s > 0.0f ? (limits->max_velocity_m_s * 1000000.0f) : 0.0f));
    protocol_put_u32_le(&body[13], (uint32_t)(limits->max_acceleration_m_s2 > 0.0f ? (limits->max_acceleration_m_s2 * 1000000.0f) : 0.0f));
    protocol_put_i32_le(&body[17], limits->soft_min_pos_um);
    protocol_put_i32_le(&body[21], limits->soft_max_pos_um);

    protocol_rsp_v2(PROTOCOL_V2_RSP_SNAPSHOT, ack_seq, 0u, body, (uint8_t)sizeof(body));
}

static void protocol_send_safety_snapshot_v2(uint16_t ack_seq)
{
    uint8_t body[13];

    body[0] = PROTOCOL_V2_STREAM_SAFETY_CONFIG;
    protocol_put_u32_le(&body[1], HAL_GetTick() * 1000u);
    protocol_put_u16_le(&body[5], protocol_build_install_flags());
    protocol_put_u16_le(&body[7], protocol_build_override_flags());
    protocol_put_u16_le(&body[9], protocol_build_behavior_flags());
    protocol_put_u16_le(&body[11], 0u);

    protocol_rsp_v2(PROTOCOL_V2_RSP_SNAPSHOT, ack_seq, 0u, body, (uint8_t)sizeof(body));
}

static void protocol_send_calibration_snapshot_v2(uint16_t ack_seq)
{
    uint8_t body[25];
    int32_t zero_pos_um = Calibration_GetZeroPosUm();
    float pitch_um = Calibration_GetPitchUm();

    body[0] = PROTOCOL_V2_STREAM_CALIBRATION;
    protocol_put_u32_le(&body[1], HAL_GetTick() * 1000u);
    body[5] = Calibration_IsValid();
    body[6] = (uint8_t)Calibration_GetSign();
    protocol_put_u16_le(&body[7], 0u);
    protocol_put_i32_le(&body[9], zero_pos_um);
    protocol_put_u32_le(&body[13], (uint32_t)(pitch_um >= 0.0f ? (pitch_um * 1000.0f) : 0.0f));
    protocol_put_i32_le(&body[17], 0);
    protocol_put_i32_le(&body[21], 0);

    protocol_rsp_v2(PROTOCOL_V2_RSP_SNAPSHOT, ack_seq, 0u, body, (uint8_t)sizeof(body));
}

static uint32_t protocol_build_capability_flags(void)
{
    uint32_t flags = 0u;

    flags |= PROTOCOL_V2_CAP_FAST_MOTION;
    flags |= PROTOCOL_V2_CAP_RUNTIME_STATUS;
    flags |= PROTOCOL_V2_CAP_LIMITS;
    flags |= PROTOCOL_V2_CAP_SAFETY_CONFIG;
    flags |= PROTOCOL_V2_CAP_CALIBRATION;
    flags |= PROTOCOL_V2_CAP_VERSION;
    flags |= PROTOCOL_V2_CAP_BINARY_COMMANDS;
    flags |= PROTOCOL_V2_CAP_HEARTBEAT;

    return flags;
}

static void protocol_send_version_snapshot_v2(uint16_t ack_seq)
{
    uint8_t body[19];
    uint8_t fw_major = 0u;
    uint8_t fw_minor = 0u;
    uint16_t fw_patch = 0u;

    protocol_parse_fw_version(&fw_major, &fw_minor, &fw_patch);

    body[0] = PROTOCOL_V2_STREAM_VERSION;
    body[1] = PROTOCOL_V2_VERSION_MAJOR;
    body[2] = PROTOCOL_V2_VERSION_MINOR;
    body[3] = fw_major;
    body[4] = fw_minor;
    protocol_put_u16_le(&body[5], fw_patch);
    protocol_put_u32_le(&body[7], 0u);
    protocol_put_u32_le(&body[11], protocol_hash_string32(FwVersion_GitHash()));
    protocol_put_u32_le(&body[15], protocol_build_capability_flags());

    protocol_rsp_v2(PROTOCOL_V2_RSP_SNAPSHOT, ack_seq, 0u, body, (uint8_t)sizeof(body));
}

static void protocol_send_snapshot_set_v2(uint16_t ack_seq, uint16_t request_mask)
{
    if (request_mask == 0u)
        request_mask = PROTOCOL_V2_REQUEST_RUNTIME_STATUS;

    if ((request_mask & PROTOCOL_V2_REQUEST_RUNTIME_STATUS) != 0u)
        protocol_send_runtime_snapshot_v2(ack_seq);
    if ((request_mask & PROTOCOL_V2_REQUEST_LIMITS) != 0u)
        protocol_send_limits_snapshot_v2(ack_seq);
    if ((request_mask & PROTOCOL_V2_REQUEST_SAFETY_CONFIG) != 0u)
        protocol_send_safety_snapshot_v2(ack_seq);
    if ((request_mask & PROTOCOL_V2_REQUEST_CALIBRATION) != 0u)
        protocol_send_calibration_snapshot_v2(ack_seq);
    if ((request_mask & PROTOCOL_V2_REQUEST_VERSION) != 0u)
        protocol_send_version_snapshot_v2(ack_seq);
}

static void protocol_handle_binary_command(const ProtocolBinaryFrame_t *frame)
{
    uint8_t command_id;
    int32_t target_um;
    uint16_t request_mask = 0u;
    uint8_t accepted = 0u;

    if ((frame == 0) || (frame->type != PROTOCOL_FRAME_COMMAND) || (frame->payload_len < 4u))
    {
        protocol_err_v2(PROTOCOL_V2_ERR_LEN_FAIL, frame ? frame->seq : 0u, 0u);
        return;
    }

    command_id = frame->payload[0];
    Watchdogs_KickComm();

    switch (command_id)
    {
    case PROTOCOL_V2_CMD_ENABLE:
        accepted = AxisControl_Enable();
        if (accepted) protocol_rsp_v2_ack(frame->seq);
        else protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_STATE_REJECTED);
        break;
    case PROTOCOL_V2_CMD_DISABLE:
        accepted = AxisControl_Disable();
        if (accepted) protocol_rsp_v2_ack(frame->seq);
        else protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_STATE_REJECTED);
        break;
    case PROTOCOL_V2_CMD_STOP:
        accepted = AxisControl_Stop();
        if (accepted) protocol_rsp_v2_ack(frame->seq);
        else protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_STATE_REJECTED);
        break;
    case PROTOCOL_V2_CMD_RESET_FAULT:
        Fault_ClearAll();
        AxisControl_RefreshState();
        protocol_rsp_v2_ack(frame->seq);
        break;
    case PROTOCOL_V2_CMD_MOVE_RELATIVE:
        if (frame->payload_len < 18u)
        {
            protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_LEN_FAIL);
            break;
        }
        target_um = protocol_read_i32_le(&frame->payload[4]);
        accepted = AxisControl_MoveRelUm(target_um);
        if (accepted) protocol_rsp_v2_ack(frame->seq);
        else protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_STATE_REJECTED);
        break;
    case PROTOCOL_V2_CMD_MOVE_ABSOLUTE:
        if (frame->payload_len < 18u)
        {
            protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_LEN_FAIL);
            break;
        }
        target_um = protocol_read_i32_le(&frame->payload[4]);
        accepted = AxisControl_MoveAbsUm(target_um);
        if (accepted) protocol_rsp_v2_ack(frame->seq);
        else protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_STATE_REJECTED);
        break;
    case PROTOCOL_V2_CMD_REQUEST_STATUS:
        if (frame->payload_len >= 6u)
            request_mask = protocol_read_u16_le(&frame->payload[4]);
        protocol_send_snapshot_set_v2(frame->seq, request_mask);
        break;
    case PROTOCOL_V2_CMD_CALIBRATION_ZERO:
        if (state.enabled || state.HomingOngoing || (AxisState_Get() == AXIS_MOTION))
            protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_STATE_REJECTED);
        else
        {
            Calibration_SetZeroPosUm(0);
            Calibration_SetValid(1u);
            state.calibrated = 1u;
            AxisControl_NotifyCalibrationComplete(1u);
            protocol_rsp_v2_ack(frame->seq);
        }
        break;
    default:
        protocol_rsp_v2_nack(frame->seq, PROTOCOL_V2_ERR_UNKNOWN_COMMAND);
        break;
    }
}

static void protocol_handle_binary_heartbeat(const ProtocolBinaryFrame_t *frame)
{
    uint8_t payload[9];

    if ((frame == 0) || (frame->type != PROTOCOL_FRAME_HEARTBEAT) || (frame->payload_len < 1u))
    {
        protocol_err_v2(PROTOCOL_V2_ERR_LEN_FAIL, frame ? frame->seq : 0u, 0u);
        return;
    }

    Watchdogs_KickComm();
    payload[0] = PROTOCOL_V2_HEARTBEAT_MCU_TO_PC;
    protocol_put_u32_le(&payload[1], HAL_GetTick());
    protocol_put_u16_le(&payload[5], frame->seq);
    protocol_put_u16_le(&payload[7], Protocol_BuildStatusFlags());
    Protocol_SendFrame(PROTOCOL_FRAME_HEARTBEAT, payload, (uint8_t)sizeof(payload));
}

void Protocol_Process(void)
{
    uint8_t pending_errors;
    char local_buf[PROTOCOL_RX_BUFFER_SIZE];
    ProtocolBinaryFrame_t binary_frame;
    char *cmd, *arg1, *arg2, *arg3;

    __disable_irq();
    pending_errors = g_pending_errors;
    g_pending_errors = 0u;
    __enable_irq();

    if ((pending_errors & PROTOCOL_PENDING_ERR_RX_OVERFLOW) != 0u)
        protocol_rsp_err("RX_OVERFLOW");
    if ((pending_errors & PROTOCOL_PENDING_ERR_CMD_OVERFLOW) != 0u)
        protocol_rsp_err("CMD_OVERFLOW");
    if ((pending_errors & PROTOCOL_PENDING_ERR_BIN_OVERFLOW) != 0u)
        protocol_err_v2(PROTOCOL_V2_ERR_BUSY, 0u, PROTOCOL_PENDING_ERR_BIN_OVERFLOW);
    if ((pending_errors & PROTOCOL_PENDING_ERR_BIN_CRC) != 0u)
        protocol_err_v2(PROTOCOL_V2_ERR_CRC_FAIL, 0u, 0u);
    if ((pending_errors & PROTOCOL_PENDING_ERR_BIN_LEN) != 0u)
        protocol_err_v2(PROTOCOL_V2_ERR_LEN_FAIL, 0u, 0u);

    Protocol_EnsureRxActive();

    __disable_irq();
    if (g_bin_count > 0u)
    {
        binary_frame = g_bin_queue[g_bin_tail];
        g_bin_tail = (uint8_t)((g_bin_tail + 1u) % PROTOCOL_BIN_QUEUE_DEPTH);
        g_bin_count--;
        __enable_irq();

        if (binary_frame.type == PROTOCOL_FRAME_COMMAND)
            protocol_handle_binary_command(&binary_frame);
        else if (binary_frame.type == PROTOCOL_FRAME_HEARTBEAT)
            protocol_handle_binary_heartbeat(&binary_frame);
        else
            protocol_err_v2(PROTOCOL_V2_ERR_UNKNOWN_TYPE, binary_frame.seq, binary_frame.type);
        return;
    }

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
