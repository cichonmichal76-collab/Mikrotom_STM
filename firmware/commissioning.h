#ifndef COMMISSIONING_H
#define COMMISSIONING_H
#include <stdint.h>
typedef struct {
    uint8_t safe_mode;
    uint8_t arming_only;
    uint8_t controlled_motion;
    uint8_t stage;
} Commissioning_t;
void Commissioning_Init(void);
void Commissioning_SetStage(uint8_t stage);
uint8_t Commissioning_GetStage(void);
uint8_t Commissioning_SafeMode(void);
uint8_t Commissioning_ArmingOnly(void);
uint8_t Commissioning_ControlledMotion(void);
void Commissioning_Apply(const Commissioning_t *cfg);
void Commissioning_SetSafeMode(uint8_t en);
void Commissioning_SetArmingOnly(uint8_t en);
void Commissioning_SetControlledMotion(uint8_t en);
uint8_t Commissioning_RunAllowed(void);
const Commissioning_t* Commissioning_Get(void);
#endif
