#ifndef AD8232_H
#define AD8232_H

#include "stm32f4xx.h"

/**
 * Initialize AD8232 ECG module
 * - sample_rate_hz: Desired sampling rate (e.g., 100Hz, 200Hz, 500Hz)
 * - Configures ADC1 on PA0
 * - Configures TIM3 to generate sampling interrupt flag
 * - Configures PA1, PA4 for Leads Off detection
 */
void AD8232_Init(uint32_t sample_rate_hz);

/**
 * Reads ADC value from AD8232 module
 * - Returns: 12-bit ADC value (0 - 4095)
 */
uint16_t AD8232_ReadValue(void);

/**
 * Checks if leads are off (disconnected)
 * - Returns: 1 if leads are off, 0 if connected
 */
uint8_t AD8232_IsLeadsOff(void);

/**
 * Global flag indicating sample ready (set in Timer interrupt)
 * Main loop will check this variable.
 */
extern volatile uint8_t ad8232_sample_ready;

#endif /* AD8232_H */