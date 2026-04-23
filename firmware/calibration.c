#include "calibration.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static Calibration_t g_calib;
void Calibration_Init(void){ Calibration_LoadDefaults(); }
void Calibration_LoadDefaults(void){ g_calib.elec_zero_pos_um = 0; g_calib.electrical_pitch_um = 30000.0f; g_calib.sign = 1; g_calib.valid = 0u; }
void Calibration_SetZeroPosUm(int32_t pos_um){ g_calib.elec_zero_pos_um = pos_um; }
void Calibration_SetPitchUm(float pitch_um){ g_calib.electrical_pitch_um = pitch_um; }
void Calibration_SetSign(int8_t sign){ g_calib.sign = (sign >= 0) ? 1 : -1; }
int32_t Calibration_GetZeroPosUm(void){ return g_calib.elec_zero_pos_um; }
float Calibration_GetPitchUm(void){ return g_calib.electrical_pitch_um; }
int8_t Calibration_GetSign(void){ return g_calib.sign; }
uint8_t Calibration_IsValid(void){ return g_calib.valid; }
void Calibration_SetValid(uint8_t en){ g_calib.valid = !!en; }
float Calibration_PositionToElecRad(int32_t pos_um)
{
    float delta_um = (float)(pos_um - g_calib.elec_zero_pos_um);
    float theta = ((2.0f * (float)M_PI) * delta_um) / g_calib.electrical_pitch_um;
    return ((float)g_calib.sign) * theta;
}
