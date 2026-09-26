#include "biquad.h"

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clamp_cutoff(float fs, float fc)
{
    float nyquist = 0.5f * fs;
    float upper = 0.45f * fs;

    if (fc < 0.001f) {
        fc = 0.001f;
    }
    if (fc > upper) {
        fc = upper;
    }
    if (fc >= nyquist) {
        fc = 0.45f * fs;
    }
    return fc;
}

static void butter_q_values(int order, float *q, int *count)
{
    int half = order / 2;
    int i;

    for (i = 0; i < half; ++i) {
        float theta = (float)M_PI * (float)(2 * i + order + 1)
                    / (float)(2 * order);
        float cosine = cosf(theta);
        q[i] = -1.0f / (2.0f * cosine);
    }
    *count = half;
}

static void rbj_lp(Biquad *filter, float fs, float fc, float q)
{
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float cw = cosf(w0);
    float sw = sinf(w0);
    float alpha = sw / (2.0f * q);
    float a0 = 1.0f + alpha;

    filter->b0 = ((1.0f - cw) * 0.5f) / a0;
    filter->b1 = (1.0f - cw) / a0;
    filter->b2 = ((1.0f - cw) * 0.5f) / a0;
    filter->a1 = (-2.0f * cw) / a0;
    filter->a2 = (1.0f - alpha) / a0;
    biquad_reset(filter);
}

static void rbj_hp(Biquad *filter, float fs, float fc, float q)
{
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float cw = cosf(w0);
    float sw = sinf(w0);
    float alpha = sw / (2.0f * q);
    float a0 = 1.0f + alpha;

    filter->b0 = ((1.0f + cw) * 0.5f) / a0;
    filter->b1 = (-(1.0f + cw)) / a0;
    filter->b2 = ((1.0f + cw) * 0.5f) / a0;
    filter->a1 = (-2.0f * cw) / a0;
    filter->a2 = (1.0f - alpha) / a0;
    biquad_reset(filter);
}

float biquad_step(Biquad *filter, float x)
{
    float y = filter->b0 * x + filter->z1;
    filter->z1 = filter->b1 * x - filter->a1 * y + filter->z2;
    filter->z2 = filter->b2 * x - filter->a2 * y;
    return y;
}

void biquad_reset(Biquad *filter)
{
    if (filter == NULL) {
        return;
    }
    filter->z1 = 0.0f;
    filter->z2 = 0.0f;
}

void cascade_reset(BiquadCascade *cascade)
{
    int i;

    if (cascade == NULL) {
        return;
    }
    for (i = 0; i < cascade->n; ++i) {
        biquad_reset(&cascade->sec[i]);
    }
}

float cascade_step(BiquadCascade *cascade, float x)
{
    float y = x;
    int i;

    if (cascade == NULL) {
        return x;
    }
    for (i = 0; i < cascade->n; ++i) {
        y = biquad_step(&cascade->sec[i], y);
    }
    return y;
}

static int normalize_order(int order)
{
    if (order < 2) {
        order = 2;
    }
    if ((order % 2) != 0) {
        ++order;
    }
    if (order > 2 * BIQUAD_MAX_CASCADE) {
        order = 2 * BIQUAD_MAX_CASCADE;
    }
    return order;
}

void design_butterworth_lp(BiquadCascade *cascade,
                           float fs, float fc, int order)
{
    float q[BIQUAD_MAX_CASCADE];
    int count = 0;
    int i;

    if (cascade == NULL || fs <= 0.0f) {
        return;
    }
    order = normalize_order(order);
    fc = clamp_cutoff(fs, fc);
    butter_q_values(order, q, &count);

    for (i = 0; i < count; ++i) {
        rbj_lp(&cascade->sec[i], fs, fc, q[i]);
    }
    cascade->n = count;
}

void design_butterworth_hp(BiquadCascade *cascade,
                           float fs, float fc, int order)
{
    float q[BIQUAD_MAX_CASCADE];
    int count = 0;
    int i;

    if (cascade == NULL || fs <= 0.0f) {
        return;
    }
    order = normalize_order(order);
    fc = clamp_cutoff(fs, fc);
    butter_q_values(order, q, &count);

    for (i = 0; i < count; ++i) {
        rbj_hp(&cascade->sec[i], fs, fc, q[i]);
    }
    cascade->n = count;
}

void design_butterworth_bp(BiquadCascade *cascade,
                           float fs, float flo, float fhi, int order)
{
    float q[BIQUAD_MAX_CASCADE];
    int count = 0;
    int i;

    if (cascade == NULL || fs <= 0.0f) {
        return;
    }
    order = normalize_order(order);
    flo = clamp_cutoff(fs, flo);
    fhi = clamp_cutoff(fs, fhi);
    if (fhi <= flo) {
        fhi = clamp_cutoff(fs, flo + 0.1f * flo);
    }
    butter_q_values(order, q, &count);

    /* HP(order) followed by LP(order), using 2*count sections. */
    for (i = 0; i < count; ++i) {
        rbj_hp(&cascade->sec[i], fs, flo, q[i]);
    }
    for (i = 0; i < count; ++i) {
        rbj_lp(&cascade->sec[count + i], fs, fhi, q[i]);
    }
    cascade->n = 2 * count;
}
