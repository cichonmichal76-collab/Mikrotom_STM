#include "commissioning.h"
#include "fault.h"
#include "eventlog.h"

static Commissioning_t g_commissioning;

static void commissioning_normalize(Commissioning_t *cfg)
{
    if (cfg == 0) return;

    cfg->safe_mode = cfg->safe_mode ? 1u : 0u;
    cfg->arming_only = cfg->arming_only ? 1u : 0u;
    cfg->controlled_motion = cfg->controlled_motion ? 1u : 0u;

    if (cfg->safe_mode)
    {
        cfg->stage = 1u;
        cfg->arming_only = 0u;
        cfg->controlled_motion = 0u;
        return;
    }

    if (cfg->controlled_motion)
    {
        cfg->stage = 3u;
        cfg->arming_only = 0u;
        return;
    }

    if (cfg->arming_only)
    {
        cfg->stage = 2u;
        cfg->controlled_motion = 0u;
        return;
    }

    switch (cfg->stage)
    {
        case 2u:
            cfg->safe_mode = 0u;
            cfg->arming_only = 1u;
            cfg->controlled_motion = 0u;
            break;
        case 3u:
            cfg->safe_mode = 0u;
            cfg->arming_only = 0u;
            cfg->controlled_motion = 1u;
            break;
        case 1u:
        default:
            cfg->stage = 1u;
            cfg->safe_mode = 1u;
            cfg->arming_only = 0u;
            cfg->controlled_motion = 0u;
            break;
    }
}

void Commissioning_Init(void)
{
    g_commissioning.safe_mode = 1u;
    g_commissioning.arming_only = 0u;
    g_commissioning.controlled_motion = 0u;
    g_commissioning.stage = 1u;
}

void Commissioning_SetStage(uint8_t stage)
{
    Commissioning_t next = g_commissioning;

    next.stage = stage;
    commissioning_normalize(&next);
    g_commissioning = next;
    EventLog_Push(EVT_STAGE_CHANGED, (int32_t)g_commissioning.stage);
}

uint8_t Commissioning_GetStage(void){ return g_commissioning.stage; }
uint8_t Commissioning_SafeMode(void){ return g_commissioning.safe_mode; }
uint8_t Commissioning_ArmingOnly(void){ return g_commissioning.arming_only; }
uint8_t Commissioning_ControlledMotion(void){ return g_commissioning.controlled_motion; }

void Commissioning_Apply(const Commissioning_t *cfg)
{
    if (cfg == 0) return;
    g_commissioning = *cfg;
    commissioning_normalize(&g_commissioning);
}

void Commissioning_SetSafeMode(uint8_t en)
{
    g_commissioning.safe_mode = !!en;
    if (g_commissioning.safe_mode)
    {
        g_commissioning.arming_only = 0u;
        g_commissioning.controlled_motion = 0u;
    }
    commissioning_normalize(&g_commissioning);
}

void Commissioning_SetArmingOnly(uint8_t en)
{
    g_commissioning.arming_only = !!en;
    if (g_commissioning.arming_only)
    {
        g_commissioning.safe_mode = 0u;
        g_commissioning.controlled_motion = 0u;
    }
    commissioning_normalize(&g_commissioning);
}

void Commissioning_SetControlledMotion(uint8_t en)
{
    g_commissioning.controlled_motion = !!en;
    if (g_commissioning.controlled_motion)
    {
        g_commissioning.safe_mode = 0u;
        g_commissioning.arming_only = 0u;
    }
    commissioning_normalize(&g_commissioning);
}

uint8_t Commissioning_RunAllowed(void)
{
    if (Fault_IsActive()) return 0u;
    if (g_commissioning.safe_mode) return 0u;
    if (!g_commissioning.controlled_motion) return 0u;
    return 1u;
}

const Commissioning_t* Commissioning_Get(void){ return &g_commissioning; }
