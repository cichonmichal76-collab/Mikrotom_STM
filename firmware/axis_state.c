#include "axis_state.h"
#include "fault.h"
#include "commissioning.h"

static AxisState_t g_axis_state = AXIS_BOOT;

void AxisState_Init(void){ g_axis_state = AXIS_BOOT; }
void AxisState_Set(AxisState_t state){ g_axis_state = state; }
AxisState_t AxisState_Get(void){ return g_axis_state; }

uint8_t AxisState_CanMove(void)
{
    if (Fault_IsActive()) return 0u;
    if (!Commissioning_RunAllowed()) return 0u;
    return (g_axis_state == AXIS_READY || g_axis_state == AXIS_ARMED || g_axis_state == AXIS_MOTION) ? 1u : 0u;
}

const char* AxisState_Name(AxisState_t state)
{
    switch (state)
    {
        case AXIS_BOOT: return "BOOT";
        case AXIS_SAFE: return "SAFE";
        case AXIS_CALIBRATION: return "CALIBRATION";
        case AXIS_READY: return "READY";
        case AXIS_ARMED: return "ARMED";
        case AXIS_MOTION: return "MOTION";
        case AXIS_STOPPING: return "STOPPING";
        case AXIS_FAULT: return "FAULT";
        case AXIS_ESTOP: return "ESTOP";
        default: return "UNKNOWN";
    }
}
