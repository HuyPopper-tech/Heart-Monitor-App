#include "ad8232.h"

/* Global flag for sample ready signal */
volatile uint8_t ad8232_sample_ready = 0;

void AD8232_Init(uint32_t sample_rate_hz) {
    /* GPIO Configuration PA0 Analog PA1 PA4 Input */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA0 Analog Mode */
    GPIOA->MODER |= (3 << 0);

    /* PA1 PA4 Input Mode */
    GPIOA->MODER &= ~( (3 << 2) | (3 << 8) );

    /* ADC1 Configuration */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* Sample Time Channel 0 */
    ADC1->SMPR2 |= (4 << 0); 

    /* Channel Sequence */
    ADC1->SQR3 = 0;

    /* Enable ADC */
    ADC1->CR2 |= ADC_CR2_ADON;

    /* TIM3 Configuration for Sample Rate Management */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* Prescaler Configuration */
    TIM3->PSC = 8399;

    /* Auto Reload Register for interrupt frequency */
    if (sample_rate_hz > 0) {
        TIM3->ARR = (10000 / sample_rate_hz) - 1;
    } else {
        TIM3->ARR = 99;
    }

    /* Enable Update Interrupt */
    TIM3->DIER |= TIM_DIER_UIE;

    /* Enable Timer */
    TIM3->CR1 |= TIM_CR1_CEN;

    /* NVIC Configuration */
    NVIC_EnableIRQ(TIM3_IRQn);
    NVIC_SetPriority(TIM3_IRQn, 1);
}

uint16_t AD8232_ReadValue(void) {
    /* Start ADC conversion */
    ADC1->CR2 |= ADC_CR2_SWSTART;

    /* Wait for End of Conversion flag */
    while (!(ADC1->SR & ADC_SR_EOC));

    /* Read and return result */
    return (uint16_t)(ADC1->DR);
}

uint8_t AD8232_IsLeadsOff(void) {
    /* Check PA1 or PA4 */
    if ((GPIOA->IDR & (1 << 1)) || (GPIOA->IDR & (1 << 4))) {
        return 1;
    }
    return 0;
}

/* Timer 3 Interrupt Handler */
void TIM3_IRQHandler(void) {
    /* Check Update Interrupt flag */
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;
        
        /* Signal to Main that sample is ready */
        ad8232_sample_ready = 1;
    }
}