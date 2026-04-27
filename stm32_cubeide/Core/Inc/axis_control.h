#ifndef AXIS_CONTROL_H
#define AXIS_CONTROL_H
#include <stdint.h>
void AxisControl_Init(void);
uint8_t AxisControl_Enable(void);
uint8_t AxisControl_Disable(void);
uint8_t AxisControl_Stop(void);
uint8_t AxisControl_QStop(void);
uint8_t AxisControl_MoveRelUm(int32_t delta_um);
uint8_t AxisControl_MoveAbsUm(int32_t target_um);
uint8_t AxisControl_FirstMoveRelUm(int32_t delta_um);
uint8_t AxisControl_FirstMoveTestActive(void);
void AxisControl_RefreshState(void);
void AxisControl_NotifyConfigChanged(void);
void AxisControl_NotifyCalibrationComplete(uint8_t success);
void AxisControl_UpdateRt(void);
void AxisControl_BackgroundTask(void);
uint8_t AxisControl_RunAllowed(void);
#endif
