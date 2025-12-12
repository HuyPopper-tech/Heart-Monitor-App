#ifndef PAN_TOMPKINS_H
#define PAN_TOMPKINS_H

#include "stm32f4xx.h"
#include "arm_math.h"

/* Algorithm configuration */
#define SAMPLE_RATE_HZ      200.0f
#define INTEGRATION_WINDOW  30

typedef struct {
    /* CMSIS-DSP Biquad filter instance for bandpass 5-15Hz */
    arm_biquad_casd_df1_inst_f32 S;
    float32_t pState[4];

    /* Derivative filter buffer */
    float32_t deriv_buff[5]; 

    /* Moving window integration buffer */
    float32_t win_buff[INTEGRATION_WINDOW];
    uint8_t win_idx;
    float32_t win_sum;

    /* Adaptive threshold variables for R-peak detection */
    float32_t threshold_i;
    float32_t signal_level;
    float32_t noise_level;
    
    /* BPM tracking variables */
    uint32_t last_beat_tick;
    uint32_t current_tick;
    uint8_t  heart_detected;
    int      current_bpm;

} PanTompkins_Handle_t;

/* Initialize Pan-Tompkins algorithm */
void PT_Init(PanTompkins_Handle_t *ht);

/* Process one ECG sample and detect heartbeat */
uint8_t PT_Process(PanTompkins_Handle_t *ht, uint16_t raw_adc);

/* Get current BPM value */
int PT_GetBPM(PanTompkins_Handle_t *ht);

#endif