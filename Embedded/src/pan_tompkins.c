#include "pan_tompkins.h"

/* IIR Biquad coefficients for bandpass filter 5-15Hz at 200Hz */
static const float32_t biquad_coeffs[5] = {
    0.09667f,   0.0f,      -0.09667f,
    1.6288f,   -0.8066f
};

void PT_Init(PanTompkins_Handle_t *ht) {
    /* Initialize CMSIS-DSP Biquad Cascade filter */
    arm_biquad_cascade_df1_init_f32(&ht->S, 1, (float32_t*)biquad_coeffs, ht->pState);

    /* Reset derivative buffer */ 
    for(int i=0; i<5; i++) ht->deriv_buff[i] = 0;
    for(int i=0; i<INTEGRATION_WINDOW; i++) ht->win_buff[i] = 0;
    
    ht->win_idx = 0;
    ht->win_sum = 0;

    /* Initialize adaptive threshold values */
    ht->threshold_i = 1000.0f; 
    ht->signal_level = 2000.0f;
    ht->noise_level = 0.0f;

    ht->last_beat_tick = 0;
    ht->current_tick = 0;
    ht->current_bpm = 0;
    ht->heart_detected = 0;
}

uint8_t PT_Process(PanTompkins_Handle_t *ht, uint16_t raw_adc) {
    float32_t input, filtered, deriv, squared, integrated;
    
    ht->current_tick++;

    /* Stage 1: Bandpass Filter 5-15Hz */
    /* Convert ADC to float and remove DC offset */
    input = (float32_t)raw_adc - 2048.0f;
    
    /* Apply CMSIS-DSP filter */
    arm_biquad_cascade_df1_f32(&ht->S, &input, &filtered, 1);

    /* Stage 2: Derivative filter */
    /* Five-point derivative: y[n] = (1/8) * (2x[n] + x[n-1] - x[n-3] - 2x[n-4]) */
    for(int i=4; i>0; i--) {
        ht->deriv_buff[i] = ht->deriv_buff[i-1];
    }
    ht->deriv_buff[0] = filtered;

    deriv = (2.0f*ht->deriv_buff[0] + ht->deriv_buff[1] - ht->deriv_buff[3] - 2.0f*ht->deriv_buff[4]) / 8.0f;

    /* Stage 3: Squaring */
    /* Emphasize QRS complex and eliminate negative values */
    squared = deriv * deriv;

    /* Stage 4: Moving Window Integration */
    /* Update sliding window sum */
    ht->win_sum -= ht->win_buff[ht->win_idx];
    ht->win_buff[ht->win_idx] = squared;
    ht->win_sum += squared;
    
    ht->win_idx++;
    if (ht->win_idx >= INTEGRATION_WINDOW) ht->win_idx = 0;

    integrated = ht->win_sum / INTEGRATION_WINDOW;

    /* Stage 5: Adaptive Thresholding and Peak Detection */
    uint8_t is_beat = 0;
    
    /* Refractory period: 200ms = 40 samples at 200Hz */
    /* Prevents false detection of T-wave or noise after QRS */
    if ((ht->current_tick - ht->last_beat_tick) > 40) {
        
        /* Check if signal exceeds threshold */
        if (integrated > ht->threshold_i) {
            
            /* QRS complex detected */
            ht->signal_level = 0.125f * integrated + 0.875f * ht->signal_level;
            is_beat = 1;
            
            /* Calculate BPM */
            uint32_t duration = ht->current_tick - ht->last_beat_tick;
            ht->last_beat_tick = ht->current_tick;
            
            /* BPM = 60 * Fs / samples_between_beats */
            float instant_bpm = (60.0f * SAMPLE_RATE_HZ) / (float)duration;
            
            /* Filter BPM with moving average */
            /* Valid BPM range: 40-200 */
            if(instant_bpm > 40 && instant_bpm < 200) {
                 ht->current_bpm = (int)(0.9f * ht->current_bpm + 0.1f * instant_bpm);
            }
        }
    }
    
    /* Update noise level if not a beat */
    if (!is_beat) {
        ht->noise_level = 0.125f * integrated + 0.875f * ht->noise_level;
    }
    
    /* Update adaptive threshold */
    /* Threshold = Noise + 0.25 * (Signal - Noise) */
    ht->threshold_i = ht->noise_level + 0.25f * (ht->signal_level - ht->noise_level);

    /* Reduce threshold if no heartbeat detected for too long */
    if ((ht->current_tick - ht->last_beat_tick) > 3000) {
         ht->threshold_i *= 0.5f;
    }

    return is_beat;
}

/* Return current BPM value */
int PT_GetBPM(PanTompkins_Handle_t *ht) {
    return ht->current_bpm;
}