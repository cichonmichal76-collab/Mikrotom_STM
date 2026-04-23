#include "config_store.h"
#include "limits.h"
#include "safety_config.h"
#include "commissioning.h"

void ConfigStore_Init(void){}
uint8_t ConfigStore_Load(void){ ConfigStore_LoadDefaults(); return 1u; }
uint8_t ConfigStore_Save(void){ return 1u; }
void ConfigStore_LoadDefaults(void)
{
    Limits_LoadDefaults();
    SafetyConfig_LoadDefaults();
    Commissioning_Init();
}
