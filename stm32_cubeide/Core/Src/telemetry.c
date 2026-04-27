#include "main.h"
#include "telemetry.h"
#include "usart.h"
#include "app_build_config.h"
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

void Telemetry_Flush(void)
{
    TelemetrySample_t s;
    uint8_t max_per_loop = 1u;
    uint8_t sent = 0u;

    if (!g_enabled)
        return;

    while ((sent < max_per_loop) && telemetry_pop(&s))
    {
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
        sent++;
    }
}
