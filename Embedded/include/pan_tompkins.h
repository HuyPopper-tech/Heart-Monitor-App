#ifndef PAN_TOMPKINS_H
#define PAN_TOMPKINS_H

#include "stm32f4xx.h"
#include "arm_math.h"
#include <stdint.h>

/*
 * Pan–Tompkins sample-by-sample pipeline (Fs = 360Hz):
 * 1) DC removal: y[n] = 0.995*y[n-1] + x[n] - x[n-1]
 * 2) Bandpass = LPF + HPF (difference equations)
 * 3) Derivative (5-point), Squaring
 * 4) Moving Window Integration (150ms)
 * 5) Local maxima + Adaptive thresholding (refractory 200ms)
 */

/* Report sampling frequency: Fs = 360Hz */
#define SAMPLE_RATE_HZ        360.0f

/* Time windows from report */
#define INTEGRATION_WINDOW    54u   /* 150ms @ 360Hz */
#define REFRACTORY_SAMPLES    72u   /* 200ms @ 360Hz */

/* Scaled integer-filter delays for Fs = 360Hz (derived from 200Hz Pan–Tompkins delays) */
#define LPF_DELAY_M           11u   /* round(6 * 360 / 200) */
#define HPF_DELAY_N           29u   /* round(16 * 360 / 200) */
#define HPF_DIV_K             58.0f /* = 2*HPF_DELAY_N */

/* Optional: threshold decay if no beat for a long time (kept stable, not continuous) */
#define NO_BEAT_TIMEOUT_S     15u
#define NO_BEAT_TIMEOUT_SAMPLES ((uint32_t)(NO_BEAT_TIMEOUT_S * SAMPLE_RATE_HZ))

typedef struct {
    /* Tick counter (one per sample) */
    uint32_t current_tick;

    /* DC removal state */
    float32_t dc_y1;
    float32_t dc_x1;

    /* LPF state */
    float32_t lpf_y1, lpf_y2;
    float32_t lpf_x_hist[2*LPF_DELAY_M + 1u];
    uint16_t  lpf_idx;

    /* HPF state (input is LPF output) */
    float32_t hpf_y1;
    float32_t hpf_x_hist[2*HPF_DELAY_N + 1u];
    uint16_t  hpf_idx;

    /* Derivative buffer */
    float32_t deriv_buff[5];

    /* Moving window integration buffer */
    float32_t win_buff[INTEGRATION_WINDOW];
    uint16_t  win_idx;
    float32_t win_sum;

    /* Local maxima detector state (on integrated signal) */
    float32_t int_prev2;
    float32_t int_prev1;

    /* Adaptive threshold variables */
    float32_t threshold_i;
    float32_t signal_level;
    float32_t noise_level;

    /* BPM tracking */
    uint32_t last_beat_tick;
    uint32_t last_decay_tick;
    int      current_bpm;


    /* ---- Expose intermediate/output signals for app ---- */
    float32_t out_x_dc;
    float32_t out_y_lpf;
    float32_t out_y_hpf;
    float32_t out_integrated;

} PanTompkins_Handle_t;

void PT_Init(PanTompkins_Handle_t *ht);

/* Process one ECG sample and return 1 if a beat is detected at a local maximum */
uint8_t PT_Process(PanTompkins_Handle_t *ht, uint16_t raw_adc);

/* Get current BPM value */
int PT_GetBPM(PanTompkins_Handle_t *ht);

#endif /* PAN_TOMPKINS_H */
