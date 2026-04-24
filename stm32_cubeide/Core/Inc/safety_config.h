#ifndef SAFETY_CONFIG_H
#define SAFETY_CONFIG_H
#include <stdint.h>
typedef struct {
    uint8_t brake_installed;
    uint8_t collision_sensor_installed;
    uint8_t ptc_installed;
    uint8_t backup_supply_installed;
    uint8_t external_interlock_installed;
    uint8_t ignore_brake_feedback;
    uint8_t ignore_collision_sensor;
    uint8_t ignore_external_interlock;
    uint8_t allow_motion_without_calibration;
} SafetyConfig_t;
void SafetyConfig_Init(void);
void SafetyConfig_LoadDefaults(void);
const SafetyConfig_t* SafetyConfig_Get(void);
void SafetyConfig_Apply(const SafetyConfig_t *cfg);
void SafetyConfig_SetBrakeInstalled(uint8_t en);
void SafetyConfig_SetCollisionInstalled(uint8_t en);
void SafetyConfig_SetPtcInstalled(uint8_t en);
void SafetyConfig_SetBackupSupplyInstalled(uint8_t en);
void SafetyConfig_SetExternalInterlockInstalled(uint8_t en);
void SafetyConfig_SetIgnoreBrakeFeedback(uint8_t en);
void SafetyConfig_SetIgnoreCollisionSensor(uint8_t en);
void SafetyConfig_SetIgnoreExternalInterlock(uint8_t en);
void SafetyConfig_SetAllowMotionWithoutCalibration(uint8_t en);
#endif
