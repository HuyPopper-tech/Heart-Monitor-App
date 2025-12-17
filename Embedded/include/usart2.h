#ifndef USART2_H
#define USART2_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

#define USART2_BAUDRATE 115200

/**
 * Initialize USART2
 * - Configures GPIOA PA2 as TX and PA3 as RX
 * - Configures USART2 with 115200 baudrate
 */
void USART2_Init(void);

/**
 * Send single character via USART2
 * - c: character to send
 */
void USART2_SendChar(char c);

/**
 * Send string via USART2
 * - str: null-terminated string to send
 */
void USART2_SendString(char *str);

/**
 * Log signals via USART2
 * - raw: raw ECG signal
 * - filtered: filtered ECG signal
 * - integrated: integrated ECG signal
 * - thresh: threshold value
 */
void USART2_LogSignals(int16_t raw, int16_t filtered, int16_t integrated, int16_t thresh);

#endif /* USART2_H */