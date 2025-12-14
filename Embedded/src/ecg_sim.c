#include "ecg_sim.h"
#include <stdlib.h>

#define ECG_SIM_FS_HZ      360.0f
#define ECG_SIM_BPM        60.0f
#define ECG_LUT_LEN        200u

/* Phase accumulators */
static float32_t ecg_phase = 0.0f;
static float32_t wander_phase = 0.0f;
static float32_t noise50hz_phase = 0.0f;

/* Standard ECG waveform lookup table one cycle normalized amplitude */
const int16_t ecg_lut[ECG_LUT_LEN] = {
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
    ecg_phase = 0.0f;
    wander_phase = 0.0f;
    noise50hz_phase = 0.0f;
}

uint16_t ECG_Sim_GetSample(void) {
    float32_t sample_val = 0.0f;

    /* Desired samples per heartbeat at current Fs */
    float32_t samples_per_cycle = (ECG_SIM_FS_HZ * 60.0f) / ECG_SIM_BPM;

    /* Advance LUT phase so that one LUT cycle spans samples_per_cycle samples */
    float32_t phase_inc = (float32_t)ECG_LUT_LEN / samples_per_cycle;
    ecg_phase += phase_inc;
    while (ecg_phase >= (float32_t)ECG_LUT_LEN) ecg_phase -= (float32_t)ECG_LUT_LEN;

    uint16_t idx = (uint16_t)ecg_phase;
    sample_val = (float32_t)ecg_lut[idx];

    /* Add baseline wander 0.5Hz simulates respiration */
    sample_val += 150.0f * arm_sin_f32(wander_phase);
    wander_phase += (2.0f * 3.1415926f * 0.5f / ECG_SIM_FS_HZ);
    if (wander_phase > 6.2831852f) wander_phase -= 6.2831852f;

    /* Add 50Hz powerline noise */
    sample_val += 30.0f * arm_sin_f32(noise50hz_phase);
    noise50hz_phase += (2.0f * 3.1415926f * 50.0f / ECG_SIM_FS_HZ);
    if (noise50hz_phase > 6.2831852f) noise50hz_phase -= 6.2831852f;

    /* Add random noise EMG simulation */
    float32_t noise = (float32_t)((rand() % 40) - 20);
    sample_val += noise;

    /* Add DC offset simulate ADC reading around 2048 */
    sample_val += 2048.0f;

    /* Clamp to 12-bit range */
    if (sample_val > 4095.0f) sample_val = 4095.0f;
    if (sample_val < 0.0f)    sample_val = 0.0f;

    return (uint16_t)sample_val;
}
