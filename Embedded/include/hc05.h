#ifndef HC05_H
#define HC05_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

/* Buffer size for sprintf in main */
#define HC05_BUFFER_SIZE 50

#define HC05_BAUDRATE       115200UL
#define APB2_CLOCK_FREQ     84000000UL

/**
 * Initialize HC-05 Bluetooth Module
 * - Configures GPIOA PA9 as TX and PA10 as RX
 * - Configures USART1 with 9600 baudrate
 */
void HC05_Init(void);

/* Send single character */
void HC05_SendChar(char c);

/* Send string */
void HC05_SendString(char *str);

#ifdef __cplusplus
}
#endif

#endif /* HC05_H */
