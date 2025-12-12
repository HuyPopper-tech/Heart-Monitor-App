#include "ecg_sim.h"
#include <stdlib.h>

/* Lookup table index */
static uint16_t sim_idx = 0;
/* Baseline wander phase */
static float32_t wander_phase = 0.0f;
/* 50Hz noise phase */
static float32_t noise50hz_phase = 0.0f;

/* Standard ECG waveform lookup table one cycle normalized amplitude */
/* Simulates P-QRS-T waveform shape */
const int16_t ecg_lut[200] = {
      0,   0,   0,   0,   0,   0,   0,   5,  10,  15,  20,  25,  30,  30,  30,  25,  20,  15,  10,   5,
      0,   0,   0,   0,   0,   0,   0,   0,   0,  -10, -20, -30, -50, -80,-100, 500,1200,1800,1200, 500,
   -100, -80, -50, -30, -20, -10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   5,  10,  15,  20,
     30,  40,  50,  60,  70,  75,  70,  60,  50,  40,  30,  20,  10,   5,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 
};

void ECG_Sim_Init(void) {
    sim_idx = 0;
    wander_phase = 0.0f;
    noise50hz_phase = 0.0f;
}

uint16_t ECG_Sim_GetSample(void) {
    float32_t sample_val = 0;

    /* Get base signal from lookup table clean ECG */
    sample_val = (float32_t)ecg_lut[sim_idx];
    
    /* Increment index wrap at 200 samples 60 BPM */
    sim_idx++;
    if (sim_idx >= 200) sim_idx = 0;

    /* Add baseline wander 0.5Hz simulates respiration */
    sample_val += 150.0f * arm_sin_f32(wander_phase);
    wander_phase += 0.015f;
    if (wander_phase > 6.28f) wander_phase = 0;

    /* Add 50Hz powerline noise */
    sample_val += 30.0f * arm_sin_f32(noise50hz_phase);
    noise50hz_phase += (50.0f * 6.28f / 200.0f);
    if (noise50hz_phase > 6.28f) noise50hz_phase -= 6.28f;

    /* Add random noise EMG simulation */
    float noise = (float)(rand() % 40) - 20.0f;
    sample_val += noise;

    /* Add DC offset simulate ADC reading around 2048 */
    sample_val += 2048.0f;

    /* Clamp to 12-bit range */
    if (sample_val > 4095) sample_val = 4095;
    if (sample_val < 0) sample_val = 0;

    return (uint16_t)sample_val;
}