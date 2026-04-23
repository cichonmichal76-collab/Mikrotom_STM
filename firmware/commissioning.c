#include "commissioning.h"
#include "fault.h"
#include "eventlog.h"

static Commissioning_t g_commissioning;

void Commissioning_Init(void)
{
    g_commissioning.safe_mode = 1u;
    g_commissioning.arming_only = 0u;
    g_commissioning.controlled_motion = 0u;
    g_commissioning.stage = 1u;
}

void Commissioning_SetStage(uint8_t stage)
{
    switch (stage)
    {
        case 1:
            g_commissioning.stage = 1u;
            g_commissioning.safe_mode = 1u;
            g_commissioning.arming_only = 0u;
            g_commissioning.controlled_motion = 0u;
            break;
        case 2:
            g_commissioning.stage = 2u;
            g_commissioning.safe_mode = 0u;
            g_commissioning.arming_only = 1u;
            g_commissioning.controlled_motion = 0u;
            break;
        case 3:
            g_commissioning.stage = 3u;
            g_commissioning.safe_mode = 0u;
            g_commissioning.arming_only = 0u;
            g_commissioning.controlled_motion = 1u;
            break;
        default:
            return;
    }
    EventLog_Push(EVT_STAGE_CHANGED, (int32_t)stage);
}

uint8_t Commissioning_GetStage(void){ return g_commissioning.stage; }
uint8_t Commissioning_SafeMode(void){ return g_commissioning.safe_mode; }
uint8_t Commissioning_ArmingOnly(void){ return g_commissioning.arming_only; }
uint8_t Commissioning_ControlledMotion(void){ return g_commissioning.controlled_motion; }
void Commissioning_SetSafeMode(uint8_t en){ g_commissioning.safe_mode = !!en; }
void Commissioning_SetArmingOnly(uint8_t en){ g_commissioning.arming_only = !!en; }
void Commissioning_SetControlledMotion(uint8_t en){ g_commissioning.controlled_motion = !!en; }
uint8_t Commissioning_RunAllowed(void)
{
    if (Fault_IsActive()) return 0u;
    if (g_commissioning.safe_mode) return 0u;
    if (!g_commissioning.controlled_motion) return 0u;
    return 1u;
}
const Commissioning_t* Commissioning_Get(void){ return &g_commissioning; }
