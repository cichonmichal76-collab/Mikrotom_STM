#ifndef EVENTLOG_H
#define EVENTLOG_H
#include <stdint.h>
#define EVENTLOG_BUFFER_SIZE 64u
typedef enum {
    EVT_NONE = 0,
    EVT_BOOT,
    EVT_STAGE_CHANGED,
    EVT_ENABLE,
    EVT_DISABLE,
    EVT_STOP,
    EVT_QSTOP,
    EVT_CALIB_START,
    EVT_CALIB_OK,
    EVT_CALIB_FAIL,
    EVT_FAULT_SET,
    EVT_FAULT_CLEAR,
    EVT_COMM_TIMEOUT,
    EVT_OVERRIDE_ON,
    EVT_OVERRIDE_OFF,
    EVT_FIRST_MOVE_TEST
} EventCode_t;
typedef struct {
    uint32_t ts_ms;
    EventCode_t code;
    int32_t value;
} EventEntry_t;
void EventLog_Init(void);
void EventLog_Push(EventCode_t code, int32_t value);
uint8_t EventLog_IsEmpty(void);
uint8_t EventLog_Count(void);
uint8_t EventLog_Pop(EventEntry_t *entry);
void EventLog_Clear(void);
const char* EventLog_CodeName(EventCode_t code);
#endif
