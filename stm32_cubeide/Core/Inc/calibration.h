#ifndef CALIBRATION_H
#define CALIBRATION_H
#include <stdint.h>
typedef struct {
    int32_t elec_zero_pos_um;
    float electrical_pitch_um;
    int8_t sign;
    uint8_t valid;
} Calibration_t;
void Calibration_Init(void);
void Calibration_LoadDefaults(void);
void Calibration_Apply(const Calibration_t *cfg);
void Calibration_SetZeroPosUm(int32_t pos_um);
void Calibration_SetPitchUm(float pitch_um);
void Calibration_SetSign(int8_t sign);
int32_t Calibration_GetZeroPosUm(void);
float Calibration_GetPitchUm(void);
int8_t Calibration_GetSign(void);
uint8_t Calibration_IsValid(void);
void Calibration_SetValid(uint8_t en);
float Calibration_PositionToElecRad(int32_t pos_um);
#endif
