#include "safety_config.h"
static SafetyConfig_t g_safety;
void SafetyConfig_Init(void){ SafetyConfig_LoadDefaults(); }
void SafetyConfig_LoadDefaults(void)
{
    g_safety.brake_installed = 0u;
    g_safety.collision_sensor_installed = 0u;
    g_safety.ptc_installed = 1u;
    g_safety.backup_supply_installed = 0u;
    g_safety.external_interlock_installed = 0u;
    g_safety.ignore_brake_feedback = 0u;
    g_safety.ignore_collision_sensor = 0u;
    g_safety.ignore_external_interlock = 0u;
    g_safety.allow_motion_without_calibration = 0u;
}
const SafetyConfig_t* SafetyConfig_Get(void){ return &g_safety; }
void SafetyConfig_Apply(const SafetyConfig_t *cfg){ if (cfg != 0) g_safety = *cfg; }
void SafetyConfig_SetBrakeInstalled(uint8_t en){ g_safety.brake_installed = !!en; }
void SafetyConfig_SetCollisionInstalled(uint8_t en){ g_safety.collision_sensor_installed = !!en; }
void SafetyConfig_SetPtcInstalled(uint8_t en){ g_safety.ptc_installed = !!en; }
void SafetyConfig_SetBackupSupplyInstalled(uint8_t en){ g_safety.backup_supply_installed = !!en; }
void SafetyConfig_SetExternalInterlockInstalled(uint8_t en){ g_safety.external_interlock_installed = !!en; }
void SafetyConfig_SetIgnoreBrakeFeedback(uint8_t en){ g_safety.ignore_brake_feedback = !!en; }
void SafetyConfig_SetIgnoreCollisionSensor(uint8_t en){ g_safety.ignore_collision_sensor = !!en; }
void SafetyConfig_SetIgnoreExternalInterlock(uint8_t en){ g_safety.ignore_external_interlock = !!en; }
void SafetyConfig_SetAllowMotionWithoutCalibration(uint8_t en){ g_safety.allow_motion_without_calibration = !!en; }
