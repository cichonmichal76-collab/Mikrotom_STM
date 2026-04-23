#ifndef FAULT_H
#define FAULT_H
#include <stdint.h>
typedef enum {
    FAULT_NONE = 0,
    FAULT_OVERCURRENT = 1,
    FAULT_OVERTEMP,
    FAULT_TRACKING,
    FAULT_OVERSPEED,
    FAULT_STALL,
    FAULT_ENCODER_INVALID,
    FAULT_UNDERVOLTAGE,
    FAULT_OVERVOLTAGE,
    FAULT_COMM_TIMEOUT,
    FAULT_NOT_CALIBRATED,
    FAULT_CONFIG_INVALID,
    FAULT_ESTOP
} FaultCode_t;
void Fault_Init(void);
void Fault_Set(FaultCode_t code);
void Fault_Clear(FaultCode_t code);
void Fault_ClearAll(void);
uint8_t Fault_IsActive(void);
FaultCode_t Fault_GetLast(void);
uint32_t Fault_GetMask(void);
const char* Fault_Name(FaultCode_t code);
#endif
