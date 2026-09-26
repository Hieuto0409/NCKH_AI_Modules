#ifndef BIQUAD_H
#define BIQUAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A 4th-order bandpass implemented as 4 cascaded second-order sections. */
#define BIQUAD_MAX_CASCADE 8

typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float z1;
    float z2;
} Biquad;

typedef struct {
    Biquad sec[BIQUAD_MAX_CASCADE];
    int n;
} BiquadCascade;

float biquad_step(Biquad *filter, float x);
void biquad_reset(Biquad *filter);
void cascade_reset(BiquadCascade *cascade);
float cascade_step(BiquadCascade *cascade, float x);

void design_butterworth_lp(BiquadCascade *cascade,
                           float fs, float fc, int order);
void design_butterworth_hp(BiquadCascade *cascade,
                           float fs, float fc, int order);
void design_butterworth_bp(BiquadCascade *cascade,
                           float fs, float flo, float fhi, int order);

#ifdef __cplusplus
}
#endif

#endif /* BIQUAD_H */
