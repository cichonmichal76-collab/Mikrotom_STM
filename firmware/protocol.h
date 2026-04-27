#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stdint.h>

typedef enum {
    PROTOCOL_FRAME_TELEMETRY = 0x01u,
    PROTOCOL_FRAME_COMMAND = 0x02u,
    PROTOCOL_FRAME_RESPONSE = 0x03u,
    PROTOCOL_FRAME_ERROR = 0x04u,
    PROTOCOL_FRAME_HEARTBEAT = 0x05u
} ProtocolFrameType_t;

void Protocol_Init(void);
void Protocol_Process(void);
void Protocol_RxCpltCallback(uint8_t byte);
void Protocol_RxErrorCallback(void);
void Protocol_EnsureRxActive(void);
void Protocol_SendLine(const char *line);
void Protocol_SendFrame(uint8_t type, const uint8_t *payload, uint8_t payload_len);
uint8_t Protocol_BinaryV2Enabled(void);
uint16_t Protocol_BuildStatusFlags(void);
uint16_t Protocol_BuildErrorFlags(void);
uint8_t Protocol_GetLastRxByte(void);
#endif
