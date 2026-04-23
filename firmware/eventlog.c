#include "main.h"
#include "eventlog.h"

static EventEntry_t g_event_buffer[EVENTLOG_BUFFER_SIZE];
static volatile uint16_t g_head = 0u;
static volatile uint16_t g_tail = 0u;
static volatile uint16_t g_count = 0u;

static uint32_t EventLog_GetTimestampMs(void){ return HAL_GetTick(); }

void EventLog_Init(void){ g_head = 0u; g_tail = 0u; g_count = 0u; }

void EventLog_Push(EventCode_t code, int32_t value)
{
    EventEntry_t entry;
    entry.ts_ms = EventLog_GetTimestampMs();
    entry.code = code;
    entry.value = value;
    g_event_buffer[g_head] = entry;
    g_head = (uint16_t)((g_head + 1u) % EVENTLOG_BUFFER_SIZE);
    if (g_count < EVENTLOG_BUFFER_SIZE) g_count++;
    else g_tail = (uint16_t)((g_tail + 1u) % EVENTLOG_BUFFER_SIZE);
}

uint8_t EventLog_IsEmpty(void){ return (g_count == 0u) ? 1u : 0u; }
uint8_t EventLog_Count(void){ return (g_count > 255u) ? 255u : (uint8_t)g_count; }

uint8_t EventLog_Pop(EventEntry_t *entry)
{
    if ((entry == 0) || (g_count == 0u)) return 0u;
    *entry = g_event_buffer[g_tail];
    g_tail = (uint16_t)((g_tail + 1u) % EVENTLOG_BUFFER_SIZE);
    g_count--;
    return 1u;
}

void EventLog_Clear(void){ g_head = 0u; g_tail = 0u; g_count = 0u; }

const char* EventLog_CodeName(EventCode_t code)
{
    switch (code)
    {
        case EVT_NONE: return "NONE";
        case EVT_BOOT: return "BOOT";
        case EVT_STAGE_CHANGED: return "STAGE_CHANGED";
        case EVT_ENABLE: return "ENABLE";
        case EVT_DISABLE: return "DISABLE";
        case EVT_STOP: return "STOP";
        case EVT_QSTOP: return "QSTOP";
        case EVT_CALIB_START: return "CALIB_START";
        case EVT_CALIB_OK: return "CALIB_OK";
        case EVT_CALIB_FAIL: return "CALIB_FAIL";
        case EVT_FAULT_SET: return "FAULT_SET";
        case EVT_FAULT_CLEAR: return "FAULT_CLEAR";
        case EVT_COMM_TIMEOUT: return "COMM_TIMEOUT";
        case EVT_OVERRIDE_ON: return "OVERRIDE_ON";
        case EVT_OVERRIDE_OFF: return "OVERRIDE_OFF";
        default: return "UNKNOWN";
    }
}
