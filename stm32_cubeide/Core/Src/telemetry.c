#include "main.h"
#include "telemetry.h"
#include "usart.h"
#include "app_build_config.h"
#include "protocol.h"
#include "motorState.h"
#include "foc.h"
#include "commissioning.h"
#include <stdio.h>
#include <string.h>

static TelemetrySample_t g_buf[TELEMETRY_BUFFER_SIZE];
static volatile uint16_t g_head = 0u;
static volatile uint16_t g_tail = 0u;
static volatile uint16_t g_count = 0u;
static volatile uint8_t g_enabled = 1u;
static uint32_t g_last_sample_ms = 0u;
#define TELEMETRY_LINE_MAX 128
static char g_tx_line[TELEMETRY_LINE_MAX];

extern volatile MotorState state;
extern volatile FieldOrientedControl foc;

#ifndef APP_TELEMETRY_MIN_PERIOD_MS
#define APP_TELEMETRY_MIN_PERIOD_MS 20u
#endif

void Telemetry_Init(void){ g_head=0u; g_tail=0u; g_count=0u; g_enabled=1u; g_last_sample_ms=0u; }
static inline uint32_t telemetry_now_ms(void){ return HAL_GetTick(); }

void Telemetry_Sample(const TelemetrySample_t *in)
{
    uint32_t now_ms;

    if (!in) return;
    if (!g_enabled) return;

    now_ms = telemetry_now_ms();
    if ((g_last_sample_ms != 0u) && ((now_ms - g_last_sample_ms) < APP_TELEMETRY_MIN_PERIOD_MS))
        return;
    g_last_sample_ms = now_ms;

    TelemetrySample_t s = *in;
    if (s.ts_ms == 0u) s.ts_ms = now_ms;
    g_buf[g_head] = s;
    g_head = (uint16_t)((g_head + 1u) % TELEMETRY_BUFFER_SIZE);
    if (g_count < TELEMETRY_BUFFER_SIZE) g_count++;
    else g_tail = (uint16_t)((g_tail + 1u) % TELEMETRY_BUFFER_SIZE);
}

static uint8_t telemetry_pop(TelemetrySample_t *out)
{
    if (out == 0) return 0u;

    __disable_irq();
    if (g_count == 0u)
    {
        __enable_irq();
        return 0u;
    }

    *out = g_buf[g_tail];
    g_tail = (uint16_t)((g_tail + 1u) % TELEMETRY_BUFFER_SIZE);
    g_count--;
    __enable_irq();
    return 1u;
}

uint16_t Telemetry_Count(void){ return g_count; }

void Telemetry_SetEnabled(uint8_t enabled)
{
    __disable_irq();
    g_enabled = enabled ? 1u : 0u;
    if (!g_enabled)
    {
        g_head = 0u;
        g_tail = 0u;
        g_count = 0u;
    }
    __enable_irq();
}

uint8_t Telemetry_IsEnabled(void){ return g_enabled; }

static void telemetry_send_line(const char *line)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)line, (uint16_t)strlen(line), 50);
}

static void telemetry_put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void telemetry_put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void telemetry_put_i16_le(uint8_t *dst, int16_t value)
{
    telemetry_put_u16_le(dst, (uint16_t)value);
}

static void telemetry_put_i32_le(uint8_t *dst, int32_t value)
{
    telemetry_put_u32_le(dst, (uint32_t)value);
}

static int32_t telemetry_read_position_counts(void)
{
    uint32_t primask = __get_PRIMASK();
    int32_t ext;
    uint16_t cnt;

    __disable_irq();
    ext = state.encoder_ext;
    cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    if (primask == 0u)
        __enable_irq();

    return (int32_t)(((uint32_t)ext << 16) | (uint32_t)cnt);
}

static void telemetry_send_frame_v2_fast(const TelemetrySample_t *s)
{
    uint8_t payload[41];
    uint32_t timestamp_us;
    int32_t position_counts;
    int16_t id_ref_mA;
    int16_t id_meas_mA;
    uint16_t vbus_mV;

    if (s == 0)
        return;

    timestamp_us = s->ts_ms * 1000u;
    position_counts = telemetry_read_position_counts();
    id_ref_mA = (int16_t)(foc.id_ref * 1000.0f);
    id_meas_mA = (int16_t)(foc.id * 1000.0f);
    vbus_mV = (uint16_t)((state.Vbus > 0.0f) ? (state.Vbus * 1000.0f) : 0.0f);

    payload[0] = 0x01u;
    telemetry_put_u32_le(&payload[1], timestamp_us);
    telemetry_put_i32_le(&payload[5], position_counts);
    telemetry_put_i32_le(&payload[9], s->pos_um);
    telemetry_put_i32_le(&payload[13], s->pos_set_um);
    telemetry_put_i32_le(&payload[17], s->vel_um_s);
    telemetry_put_i32_le(&payload[21], s->vel_set_um_s);
    telemetry_put_i16_le(&payload[25], s->iq_ref_mA);
    telemetry_put_i16_le(&payload[27], s->iq_meas_mA);
    telemetry_put_i16_le(&payload[29], id_ref_mA);
    telemetry_put_i16_le(&payload[31], id_meas_mA);
    telemetry_put_u16_le(&payload[33], vbus_mV);
    payload[35] = s->state;
    payload[36] = Commissioning_GetStage();
    telemetry_put_u16_le(&payload[37], Protocol_BuildStatusFlags());
    telemetry_put_u16_le(&payload[39], Protocol_BuildErrorFlags());

    Protocol_SendFrame(PROTOCOL_FRAME_TELEMETRY, payload, (uint8_t)sizeof(payload));
}

void Telemetry_Flush(void)
{
    TelemetrySample_t s;
    uint8_t max_per_loop = 1u;
    uint8_t sent = 0u;

    if (!g_enabled)
        return;

    while ((sent < max_per_loop) && telemetry_pop(&s))
    {
#if APP_PROTOCOL_BINARY_V2_TELEMETRY
        telemetry_send_frame_v2_fast(&s);
#else
        int len = snprintf(g_tx_line, TELEMETRY_LINE_MAX,
            "TEL,%lu,%ld,%ld,%ld,%ld,%d,%d,%u,%u\n",
            (unsigned long)s.ts_ms,
            (long)s.pos_um,
            (long)s.pos_set_um,
            (long)s.vel_um_s,
            (long)s.vel_set_um_s,
            (int)s.iq_ref_mA,
            (int)s.iq_meas_mA,
            (unsigned int)s.state,
            (unsigned int)s.fault
        );
        if (len > 0) telemetry_send_line(g_tx_line);
#endif
        sent++;
    }
}
