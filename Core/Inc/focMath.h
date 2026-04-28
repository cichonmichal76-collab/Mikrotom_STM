/*
 * focMath.h
 *
 *  Created on: Mar 20, 2026
 *      Author: lukasz.sklodowski
 */

#ifndef INC_FOCMATH_H_
#define INC_FOCMATH_H_

#include <stdint.h>
#include "math.h"

static inline float fast_atan2f_precise(float y, float x)
{
    float ax = fabsf(x);
    float ay = fabsf(y);

    if ((ax + ay) == 0.0f) return 0.0f;

    int invert = (ay > ax);
    float t = invert ? ax / ay : ay / ax;

    const float a0 = 1.00000011921f;
    const float a1 = -0.33333145280f;
    const float a2 = 0.19993550850f;
    const float a3 = -0.14208899436f;

    float t2 = t * t;
    float ang = t * (a0 + t2 * (a1 + t2 * (a2 + t2 * a3)));

    ang = invert ? (1.57079632679f - ang) : ang;

    if (x < 0.0f) ang = 3.14159265359f - ang;
    if (y < 0.0f) ang = -ang;

    return ang;
}

#endif /* INC_FOCMATH_H_ */
