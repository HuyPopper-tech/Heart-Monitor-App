#include "pan_tompkins.h"
#include <string.h>

/* Helper: circular history access for LPF/HPF */
static inline float32_t hist_get(const float32_t *hist, uint16_t size, uint16_t idx, uint16_t delay) {
    /* hist[idx] is the newest sample written at current idx */
    int32_t pos = (int32_t)idx - (int32_t)delay;
    while (pos < 0) pos += (int32_t)size;
    return hist[(uint16_t)pos];
}

/*
 * Difference equations adjusted for Fs = 360Hz by scaling Pan–Tompkins integer-filter delays:
 *  - LPF (scaled): yLP[n] = 2yLP[n-1] - yLP[n-2] + x[n] - 2x[n-M] + x[n-2M],  M=11
 *  - HPF (scaled): yHP[n] = yHP[n-1] - v[n]/(2N) + v[n-N] - v[n-(N+1)] + v[n-2N]/(2N), N=29
 * where v[n] is LPF output (input to HPF).
 */

void PT_Init(PanTompkins_Handle_t *ht) {
    memset(ht, 0, sizeof(*ht));

    /* Conservative start-up values (will adapt) */
    ht->threshold_i  = 1000.0f;
    ht->signal_level = 2000.0f;
    ht->noise_level  = 0.0f;

    ht->last_beat_tick  = 0;
    ht->last_decay_tick = 0;
    ht->current_bpm     = 0;

    /* Seed local maxima tracker */
    ht->int_prev2 = 0.0f;
    ht->int_prev1 = 0.0f;
}

uint8_t PT_Process(PanTompkins_Handle_t *ht, uint16_t raw_adc) {
    ht->current_tick++;

    /* Convert ADC to centered float (rough DC at mid-scale). Report then applies DC-removal filter. */
    float32_t x = (float32_t)raw_adc - 2048.0f;

    /* Stage 0: DC removal (report equation) */
    /* y[n] = 0.995*y[n-1] + x[n] - x[n-1] */
    float32_t x_dc = 0.995f * ht->dc_y1 + (x - ht->dc_x1);
    ht->dc_x1 = x;
    ht->dc_y1 = x_dc;

    /* Stage 1a: LPF (scaled integer structure) */
    const uint16_t lpf_size = (uint16_t)(2u*LPF_DELAY_M + 1u);

    /* write newest x_dc */
    ht->lpf_x_hist[ht->lpf_idx] = x_dc;

    float32_t x_n    = hist_get(ht->lpf_x_hist, lpf_size, ht->lpf_idx, 0);
    float32_t x_n_M  = hist_get(ht->lpf_x_hist, lpf_size, ht->lpf_idx, LPF_DELAY_M);
    float32_t x_n_2M = hist_get(ht->lpf_x_hist, lpf_size, ht->lpf_idx, 2u*LPF_DELAY_M);

    float32_t y_lpf = 2.0f*ht->lpf_y1 - ht->lpf_y2 + x_n - 2.0f*x_n_M + x_n_2M;

    ht->lpf_y2 = ht->lpf_y1;
    ht->lpf_y1 = y_lpf;

    ht->lpf_idx++;
    if (ht->lpf_idx >= lpf_size) ht->lpf_idx = 0;

    /* Stage 1b: HPF (scaled) - input is LPF output */
    const uint16_t hpf_size = (uint16_t)(2u*HPF_DELAY_N + 1u);

    ht->hpf_x_hist[ht->hpf_idx] = y_lpf;

    float32_t v_n      = hist_get(ht->hpf_x_hist, hpf_size, ht->hpf_idx, 0);
    float32_t v_n_N    = hist_get(ht->hpf_x_hist, hpf_size, ht->hpf_idx, HPF_DELAY_N);
    float32_t v_n_N1   = hist_get(ht->hpf_x_hist, hpf_size, ht->hpf_idx, (uint16_t)(HPF_DELAY_N + 1u));
    float32_t v_n_2N   = hist_get(ht->hpf_x_hist, hpf_size, ht->hpf_idx, (uint16_t)(2u*HPF_DELAY_N));

    float32_t y_hpf = ht->hpf_y1
                      - (v_n / HPF_DIV_K)
                      + v_n_N
                      - v_n_N1
                      + (v_n_2N / HPF_DIV_K);

    ht->hpf_y1 = y_hpf;

    ht->hpf_idx++;
    if (ht->hpf_idx >= hpf_size) ht->hpf_idx = 0;

    /* Stage 2: Derivative (5-point) */
    for (int i = 4; i > 0; i--) ht->deriv_buff[i] = ht->deriv_buff[i - 1];
    ht->deriv_buff[0] = y_hpf;

    float32_t deriv = (2.0f*ht->deriv_buff[0]
                     + ht->deriv_buff[1]
                     - ht->deriv_buff[3]
                     - 2.0f*ht->deriv_buff[4]) / 8.0f;

    /* Stage 3: Squaring */
    float32_t squared = deriv * deriv;

    /* Stage 4: Moving Window Integration (150ms @ 360Hz -> 54 samples) */
    ht->win_sum -= ht->win_buff[ht->win_idx];
    ht->win_buff[ht->win_idx] = squared;
    ht->win_sum += squared;

    ht->win_idx++;
    if (ht->win_idx >= INTEGRATION_WINDOW) ht->win_idx = 0;

    float32_t integrated = ht->win_sum / (float32_t)INTEGRATION_WINDOW;

    /* Stage 5: Local maxima detection on integrated signal (per report) */
    uint8_t is_beat = 0;

    float32_t int_curr = integrated;
    /* A local maximum occurs at int_prev1 if int_prev1 > int_prev2 and int_prev1 > int_curr */
    if ((ht->int_prev1 > ht->int_prev2) && (ht->int_prev1 > int_curr)) {
        float32_t peak_val = ht->int_prev1;
        uint32_t  peak_tick = ht->current_tick - 1u; /* peak at previous sample */

        /* Refractory period: 200ms */
        if ((peak_tick - ht->last_beat_tick) > REFRACTORY_SAMPLES) {
            if (peak_val > ht->threshold_i) {
                /* QRS detected */
                ht->signal_level = 0.125f * peak_val + 0.875f * ht->signal_level;
                is_beat = 1;

                /* BPM */
                uint32_t duration = peak_tick - ht->last_beat_tick;
                ht->last_beat_tick = peak_tick;

                if (duration > 0) {
                    float32_t instant_bpm = (60.0f * SAMPLE_RATE_HZ) / (float32_t)duration;
                    if (instant_bpm > 40.0f && instant_bpm < 200.0f) {
                        ht->current_bpm = (int)(0.9f * (float32_t)ht->current_bpm + 0.1f * instant_bpm);
                    }
                }
            } else {
                /* Not a QRS peak -> treat as noise peak */
                ht->noise_level = 0.125f * peak_val + 0.875f * ht->noise_level;
            }

            /* Update threshold after classifying the peak (report equation) */
            ht->threshold_i = ht->noise_level + 0.25f * (ht->signal_level - ht->noise_level);
        } else {
            /* Within refractory: ignore for beat detection, but still update noise mildly */
            ht->noise_level = 0.125f * peak_val + 0.875f * ht->noise_level;
            ht->threshold_i = ht->noise_level + 0.25f * (ht->signal_level - ht->noise_level);
        }
    }

    /* Threshold decay (stable): if no beat for too long, reduce threshold periodically */
    if ((ht->current_tick - ht->last_beat_tick) > NO_BEAT_TIMEOUT_SAMPLES) {
        uint32_t decay_period = (uint32_t)SAMPLE_RATE_HZ; /* once per second */
        if ((ht->current_tick - ht->last_decay_tick) > decay_period) {
            ht->threshold_i *= 0.5f;
            ht->last_decay_tick = ht->current_tick;
        }
    } else {
        ht->last_decay_tick = ht->current_tick;
    }

    /* shift local maxima tracker */
    ht->int_prev2 = ht->int_prev1;
    ht->int_prev1 = int_curr;

    return is_beat;
}

int PT_GetBPM(PanTompkins_Handle_t *ht) {
    return ht->current_bpm;
}
