#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stdint.h>
void Protocol_Init(void);
void Protocol_Process(void);
void Protocol_RxCpltCallback(uint8_t byte);
void Protocol_SendLine(const char *line);
uint8_t Protocol_GetLastRxByte(void);
#endif
