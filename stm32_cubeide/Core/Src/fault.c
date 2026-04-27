#include "fault.h"
#include "eventlog.h"
#include "axis_state.h"

static volatile uint32_t g_fault_mask = 0u;
static volatile FaultCode_t g_last_fault = FAULT_NONE;

static uint32_t fault_to_mask(FaultCode_t code)
{
    if (code == FAULT_NONE) return 0u;
    return (1UL << ((uint32_t)code - 1UL));
}

static FaultCode_t fault_pick_reported(uint32_t mask)
{
    int32_t code;

    if (mask == 0u) return FAULT_NONE;

    for (code = (int32_t)FAULT_ESTOP; code >= (int32_t)FAULT_OVERCURRENT; code--)
    {
        FaultCode_t fault = (FaultCode_t)code;
        if ((mask & fault_to_mask(fault)) != 0u)
            return fault;
    }

    return FAULT_NONE;
}

void Fault_Init(void){ g_fault_mask = 0u; g_last_fault = FAULT_NONE; }

void Fault_Set(FaultCode_t code)
{
    uint32_t mask;

    if (code == FAULT_NONE) return;
    mask = fault_to_mask(code);
    if ((g_fault_mask & mask) != 0u)
    {
        AxisState_Set((code == FAULT_ESTOP) ? AXIS_ESTOP : AXIS_FAULT);
        return;
    }

    g_fault_mask |= mask;
    g_last_fault = code;
    EventLog_Push(EVT_FAULT_SET, (int32_t)code);
    AxisState_Set((code == FAULT_ESTOP) ? AXIS_ESTOP : AXIS_FAULT);
}

void Fault_Clear(FaultCode_t code)
{
    if (code == FAULT_NONE) return;
    g_fault_mask &= ~fault_to_mask(code);
    EventLog_Push(EVT_FAULT_CLEAR, (int32_t)code);
    g_last_fault = fault_pick_reported(g_fault_mask);
    if (g_fault_mask == 0u)
        AxisState_Set(AXIS_SAFE);
}

void Fault_ClearAll(void)
{
    g_fault_mask = 0u;
    g_last_fault = FAULT_NONE;
    EventLog_Push(EVT_FAULT_CLEAR, 0);
    AxisState_Set(AXIS_SAFE);
}

uint8_t Fault_IsActive(void){ return g_fault_mask ? 1u : 0u; }
FaultCode_t Fault_GetLast(void){ return g_last_fault; }
uint32_t Fault_GetMask(void){ return g_fault_mask; }

const char* Fault_Name(FaultCode_t code)
{
    switch (code)
    {
        case FAULT_NONE: return "NONE";
        case FAULT_OVERCURRENT: return "OVERCURRENT";
        case FAULT_OVERTEMP: return "OVERTEMP";
        case FAULT_TRACKING: return "TRACKING";
        case FAULT_OVERSPEED: return "OVERSPEED";
        case FAULT_STALL: return "STALL";
        case FAULT_ENCODER_INVALID: return "ENCODER_INVALID";
        case FAULT_UNDERVOLTAGE: return "UNDERVOLTAGE";
        case FAULT_OVERVOLTAGE: return "OVERVOLTAGE";
        case FAULT_COMM_TIMEOUT: return "COMM_TIMEOUT";
        case FAULT_NOT_CALIBRATED: return "NOT_CALIBRATED";
        case FAULT_CONFIG_INVALID: return "CONFIG_INVALID";
        case FAULT_ESTOP: return "ESTOP";
        default: return "UNKNOWN";
    }
}
