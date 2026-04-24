#ifndef WATCHDOGS_H
#define WATCHDOGS_H
#include <stdint.h>
typedef struct {
    uint32_t last_comm_ms;
    uint32_t comm_timeout_ms;
    uint8_t comm_timeout_active;
} Watchdogs_t;
void Watchdogs_Init(void);
void Watchdogs_SetCommTimeout(uint32_t timeout_ms);
void Watchdogs_KickComm(void);
void Watchdogs_Update(void);
uint8_t Watchdogs_IsCommTimedOut(void);
const Watchdogs_t* Watchdogs_Get(void);
#endif
