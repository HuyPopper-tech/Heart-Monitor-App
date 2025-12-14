#include "ad8232.h"

/* Global flag for sample ready signal */
volatile uint8_t  ad8232_sample_ready = 0;
volatile uint16_t ad8232_latest_adc   = 0;

void AD8232_Init(uint32_t sample_rate_hz) {
    /* GPIO Configuration PA0 Analog, PA1/PA4 Input */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA0 Analog Mode */
    GPIOA->MODER |= (3u << 0);

    /* PA1 PA4 Input Mode */
    GPIOA->MODER &= ~((3u << 2) | (3u << 8));

    /* ADC1 Configuration */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* Sample Time Channel 0 */
    ADC1->SMPR2 |= (4u << 0);

    /* Channel Sequence */
    ADC1->SQR3 = 0;

    /* Enable ADC */
    ADC1->CR2 |= ADC_CR2_ADON;

    /* TIM3 Configuration for Sample Rate Management */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /*
     * Use timer tick = 1MHz for good ARR resolution:
     *   TIM3_CLK = 84MHz (APB1 timer clock on STM32F4 when APB1 prescaler != 1)
     *   PSC = 83 => 84MHz/(83+1)=1MHz
     * ARR = round(1e6 / Fs) - 1
     */
    TIM3->PSC = 83u;

    if (sample_rate_hz > 0u) {
        uint32_t arr = (1000000u + (sample_rate_hz / 2u)) / sample_rate_hz;
        if (arr > 0u) arr -= 1u;
        if (arr > 0xFFFFu) arr = 0xFFFFu;
        TIM3->ARR = (uint16_t)arr;
    } else {
        TIM3->ARR = 2777u; /* default ~360Hz */
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
    if ((GPIOA->IDR & (1u << 1)) || (GPIOA->IDR & (1u << 4))) {
        return 1;
    }
    return 0;
}

/* Timer 3 Interrupt Handler (report flow: trigger ADC, wait EOC, read sample) */
void TIM3_IRQHandler(void) {
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;

        /* 1) Trigger ADC conversion */
        ADC1->CR2 |= ADC_CR2_SWSTART;

        /* 2) Wait for EOC (few us) */
        while (!(ADC1->SR & ADC_SR_EOC));

        /* 3) Read ADC */
        ad8232_latest_adc = (uint16_t)(ADC1->DR);

        /* 4) Signal main loop */
        ad8232_sample_ready = 1;
    }
}
