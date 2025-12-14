#ifndef ECG_SIM_H
#define ECG_SIM_H

#include "stm32f4xx.h"
#include "arm_math.h"
#include <stdint.h>

/* Initialize ECG simulator */
void ECG_Sim_Init(void);

/* Get simulated ECG sample returns 12-bit value 0-4095 */
uint16_t ECG_Sim_GetSample(void);

#endif /* ECG_SIM_H */
