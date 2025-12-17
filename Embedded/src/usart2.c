#include "usart2.h"

void USART2_Init(void) {
    /* Enable Clock for GPIOA and USART2 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN; 

    /* Configure GPIO PA2 (TX) and PA3 (RX) as Alternate Function */
    GPIOA->MODER &= ~((3U << 4) | (3U << 6)); 
    /* Set mode AF (10) cho PA2, PA3 */
    GPIOA->MODER |=  ((2U << 4) | (2U << 6)); 

    /* Alternate function selection */
    GPIOA->AFR[0] &= ~((0xFU << 8) | (0xFU << 12));
    GPIOA->AFR[0] |=  ((0x7U << 8) | (0x7U << 12));

    /* Reset CR1 */
    USART2->CR1 = 0; 

    /* Configure Baudrate. 
     * PCLK1 = 42MHz 
     * Baud = 115200
     * USARTDIV = 42000000 / 115200 = 364.58
     */
    uint32_t pclk1 = 42000000U;
    uint32_t brr = (pclk1 + (USART2_BAUDRATE / 2)) / USART2_BAUDRATE;
    USART2->BRR = (uint16_t)brr;

    /* Enable TX, RX and USART */
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

void USART2_SendChar(char c) {
    /* Wait until the transmit data register is empty */
    while (!(USART2->SR & USART_SR_TXE));
    /* Write data to the DR register */
    USART2->DR = c;
}

void USART2_SendString(char *str) {
    while (*str) {
        USART2_SendChar(*str++);
    }
}

void USART2_LogSignals(int16_t raw, int16_t filtered, int16_t integrated, int16_t thresh) {
    char buff[64];
    /* Format CSV: Col1,Col2,Col3,Col4 new line */
    sprintf(buff, "%d,%d,%d,%d\r\n", raw, filtered, integrated, thresh);
    USART2_SendString(buff);
}