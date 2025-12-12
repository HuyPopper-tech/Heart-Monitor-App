#include "main.h"
#include "hc05.h"
#include "ad8232.h"
#include "pan_tompkins.h"
#include "ecg_sim.h"
#include <stdio.h>

/* Set to 1 to use ECG simulator instead of AD8232 */
#define USE_ECG_SIM 1

void SystemClock_Config(void);
void Error_Handler(void);

char msg_buffer[HC05_BUFFER_SIZE];
PanTompkins_Handle_t pt_handle;

/* Current BPM value */
int current_bpm = 0; 

int main(void) {
    HAL_Init();
    SystemClock_Config();

    /* Initialize HC-05 */
    HC05_Init();

    /* Initialize AD8232 with 100Hz sample rate */
    AD8232_Init(100); 

#if USE_ECG_SIM
    /* Initialize ECG Simulator */
    ECG_Sim_Init();
#endif
    /* Initialize Pan-Tompkins algorithm */
    PT_Init(&pt_handle);
    while (1) {
        if (ad8232_sample_ready == 1) {
            ad8232_sample_ready = 0;
            
            uint16_t ecg_val = 0;

#if USE_ECG_SIM
            /* Simulation mode */
            /* Skip leads off check */
            ecg_val = ECG_Sim_GetSample();
            
            /* Process simulated signal with Pan-Tompkins */
            PT_Process(&pt_handle, ecg_val);
            
            /* Get BPM */
            int bpm = PT_GetBPM(&pt_handle);

            /* Send data */
            sprintf(msg_buffer, "%d,%d\r\n", ecg_val, bpm);
            HC05_SendString(msg_buffer);

#else
            /* Real hardware mode */
            if (AD8232_IsLeadsOff()) {
                sprintf(msg_buffer, "0,0\r\n");
                HC05_SendString(msg_buffer);
                pt.current_bpm = 0; 
            } else {
                ecg_val = AD8232_ReadValue();
                PT_Process(&pt, ecg_val);
                int bpm = PT_GetBPM(&pt);
                sprintf(msg_buffer, "%d,%d\r\n", ecg_val, bpm);
                HC05_SendString(msg_buffer);
            }
#endif
        }
    }
}

/* System Clock Configuration */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    /* Initialize RCC Oscillators */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Initialize CPU AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/* GPIO Initialization Function */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO Ports Clock */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    /* Configure B1 Pin */
    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /* Configure LD2 Pin */
    GPIO_InitStruct.Pin = LD2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

/* Error Handler */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
    }
}
#ifdef USE_FULL_ASSERT
/* Assert failed handler */
void assert_failed(uint8_t* file, uint32_t line) {
}
#endif /* USE_FULL_ASSERT */
