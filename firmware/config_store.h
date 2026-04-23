#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H
#include <stdint.h>
typedef struct { uint32_t version; uint32_t crc; } ConfigStoreHeader_t;
void ConfigStore_Init(void);
uint8_t ConfigStore_Load(void);
uint8_t ConfigStore_Save(void);
void ConfigStore_LoadDefaults(void);
#endif
