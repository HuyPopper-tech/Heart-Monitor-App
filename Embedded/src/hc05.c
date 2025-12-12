#include "hc05.h"

void HC05_Init(void) {
    /* Enable GPIOA and USART1 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* Configure GPIO PA9 TX and PA10 RX */
    /* Mode Alternate Function */
    GPIOA->MODER &= ~((3 << 18) | (3 << 20));
    GPIOA->MODER |=  ((2 << 18) | (2 << 20));

    /* Output Type Push-Pull */
    GPIOA->OTYPER &= ~((1 << 9) | (1 << 10));

    /* Speed High Speed */
    GPIOA->OSPEEDR |= ((3 << 18) | (3 << 20));

    /* Alternate Function Mapping AF07 for USART1 */
    GPIOA->AFR[1] &= ~((0xF << 4) | (0xF << 8));
    GPIOA->AFR[1] |=  ((0x7 << 4) | (0x7 << 8));

    /* Configure USART1 */
    USART1->CR1 = 0;
    
    /* Baudrate configuration */
    uint32_t brr_val = (APB2_CLOCK_FREQ + (HC05_BAUDRATE / 2)) / HC05_BAUDRATE;
    USART1->BRR = (uint16_t)brr_val;

    /* Enable Transmitter Receiver and USART */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void HC05_SendChar(char c) {
    /* Wait until transmit data register is empty */
    while (!(USART1->SR & USART_SR_TXE));
    
    /* Write data to register */
    USART1->DR = c;
}

void HC05_SendString(char *str) {
    while (*str) {
        HC05_SendChar(*str++);
    }
}