#ifndef AXIS_STATE_H
#define AXIS_STATE_H
#include <stdint.h>
typedef enum {
    AXIS_BOOT = 0,
    AXIS_SAFE,
    AXIS_CALIBRATION,
    AXIS_READY,
    AXIS_ARMED,
    AXIS_MOTION,
    AXIS_STOPPING,
    AXIS_FAULT,
    AXIS_ESTOP
} AxisState_t;
void AxisState_Init(void);
void AxisState_Set(AxisState_t state);
AxisState_t AxisState_Get(void);
uint8_t AxisState_CanMove(void);
const char* AxisState_Name(AxisState_t state);
#endif
