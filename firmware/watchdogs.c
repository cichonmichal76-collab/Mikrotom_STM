#include "main.h"
#include "watchdogs.h"
#include "fault.h"
#include "axis_state.h"
#include "commissioning.h"
#include "eventlog.h"

static Watchdogs_t g_watchdogs;
static inline uint32_t watchdogs_now_ms(void){ return HAL_GetTick(); }

void Watchdogs_Init(void)
{
    g_watchdogs.last_comm_ms = watchdogs_now_ms();
    g_watchdogs.comm_timeout_ms = 1000u;
    g_watchdogs.comm_timeout_active = 0u;
}

void Watchdogs_SetCommTimeout(uint32_t timeout_ms){ g_watchdogs.comm_timeout_ms = timeout_ms; }

void Watchdogs_KickComm(void)
{
    g_watchdogs.last_comm_ms = watchdogs_now_ms();
    if (g_watchdogs.comm_timeout_active)
    {
        g_watchdogs.comm_timeout_active = 0u;
        Fault_Clear(FAULT_COMM_TIMEOUT);
    }
}

void Watchdogs_Update(void)
{
    uint32_t now = watchdogs_now_ms();
    uint32_t dt = now - g_watchdogs.last_comm_ms;
    if (dt > g_watchdogs.comm_timeout_ms)
    {
        if (!g_watchdogs.comm_timeout_active)
        {
            g_watchdogs.comm_timeout_active = 1u;
            Fault_Set(FAULT_COMM_TIMEOUT);
            EventLog_Push(EVT_COMM_TIMEOUT, (int32_t)dt);
            Commissioning_SetControlledMotion(0u);
        }
    }
}

uint8_t Watchdogs_IsCommTimedOut(void){ return g_watchdogs.comm_timeout_active; }
const Watchdogs_t* Watchdogs_Get(void){ return &g_watchdogs; }
