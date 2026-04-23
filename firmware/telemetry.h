#ifndef TELEMETRY_H
#define TELEMETRY_H
#include <stdint.h>
#define TELEMETRY_BUFFER_SIZE 128u
typedef struct {
    uint32_t ts_ms;
    int32_t pos_um;
    int32_t pos_set_um;
    int32_t vel_um_s;
    int32_t vel_set_um_s;
    int16_t iq_ref_mA;
    int16_t iq_meas_mA;
    uint8_t state;
    uint8_t fault;
} TelemetrySample_t;
void Telemetry_Init(void);
void Telemetry_Sample(const TelemetrySample_t *sample);
void Telemetry_Flush(void);
uint16_t Telemetry_Count(void);
#endif
