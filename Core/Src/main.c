/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Serial.h"
#include "roboClaw.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_uart4_rx;
DMA_HandleTypeDef hdma_uart5_rx;
DMA_HandleTypeDef hdma_uart5_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
SERIAL_HandleTypeDef *hserial_uart5;
SERIAL_HandleTypeDef *hserial_uart3;

// mc1 = left drive  (0x80)
// mc2 = right drive (0x81)
// mc3 = weapon      (0x82)
RoboClaw_HandleTypeDef hroboclaw_mc1;
RoboClaw_HandleTypeDef hroboclaw_mc2;
RoboClaw_HandleTypeDef hroboclaw_mc3;


// RoboClaw addresses
//go into basicmicro and configure these
//also configure to packet serial
#define LEFT_RC   0x80
#define RIGHT_RC  0x81
#define WEAPON_RC 0x82

// Debug buffer
uint8_t debug_buff[20000];

//variables to read back
uint32_t m1_enc_cnt;
uint32_t m1_speed;
uint8_t  rc_status;
bool     valid;
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_UART5_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_UART4_Init(void);
/* USER CODE BEGIN PFP */
// 1. Wrap initialization into an init function
void RoboClaw_Init_All(void);
void Stop_All_Motors(void);
char Keypad_Scan(void);

//*************************************NEXT STEPS TO CODE*******************************************************************************
//1. instead of initalizing every roboclaw, wrap that into an init function so we can call those same lines in main over and over
//2. instead of having it just run for 3 seconds (HAL_Delay(3000)) interface with keypad
//3. take given keypad code and initialize the gpio pins for it in the ioc file
//4. import the pushed2release and release2pushed code files from 316 to detect held button presses
//5. move all the logic in main to the while loop so it keeps polling the keypad for action
//6. map the key states to the motor commands within the while loop (look at old keypad code for old while loops)
//   2 held down = ForwardM1/M2 on left_rc ForwardM1/M2 on right_rc
//   4 held down = BackwardM1/M2 on left_rc BackwardM1/M2 on right_rc
//   5 held down = ForwardM1 on weapon_rc
//   3 held down = left tank turn = BackwardsM1/M2 on left_rc ForwardM1/M2 on right_rc
//   6 held down = right tank turn = ForwardM1/M2 on right_rc BackwardM1/M2 on left_rc
//   **upon release of these buttons, send ForwardM1/M2 (0) as speed command**
//7. make sure key_down -> start motors, key_release -> stop motors
//8. not implementing simulatneous control (like the weapon and the drive cannot happen at the same time) (too hard for time given)
// **************************************************************************************************************************************

//************************************* possible errors while debugging today *********************************************************
//1. when calling ForwardM1 and ForwardM2, if both motors dont spin the same direction, that means one of the motors is wired backwards
//			- swap the wires going to M1A and M1B and re-test
//			- make sure that when calling FORWARD, both motors are spinning the same direction.
//2. make sure all roboclaws share a common ground with the STM (can be differnet ground pins on the STM)
//3. make sure all roboclaws are set to packet serial mode, 115200 baude rate, with correct addresses in decimal in basicmicro
//*************************************************************************************************************************************

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define MAX_SPEED 15  /* 0-127; keep low for a bench test */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_UART5_Init();
  MX_USART3_UART_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */
    //initializing the printing
          hserial_uart3 = serial_init(&huart3);
          serial_write(hserial_uart3, (uint8_t*)"=== RoboClaw Control Start ===\r\n", 32);

          //initialize the uart for the roboclaw
          hserial_uart5 = serial_init(&huart5);

          // 1. Initializing roboclaws via wrapper function
          RoboClaw_Init_All();

          serial_write(hserial_uart3, (uint8_t*)"System Ready. Use Keypad to drive.\r\n", 36);

          // Variables for debouncing/state tracking
          char last_stable_key = 0;
          char current_detection = 0;
          int confidence = 0;
          const int THRESHOLD = 3; // Confidence threshold
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
            while (1)
            {
                // 5. Poll the keypad for action
                char raw_reading = Keypad_Scan();

                // 4. detect held button presses via confidence debouncing
                if (raw_reading == current_detection && raw_reading != 0) {
                    confidence++;
                } else {
                    confidence = 0;
                    current_detection = raw_reading;
                }

                // Update state if we hit threshold; if raw is 0, reset immediately to stop
                if (confidence >= THRESHOLD) {
                    last_stable_key = current_detection;
                } else if (raw_reading == 0) {
                    last_stable_key = 0;
                }

                // 6. map the key states to the motor commands within the while loop
                if (last_stable_key == '2') {
                    // 2 = FORWARD
                    ForwardM1(&hroboclaw_mc1, MAX_SPEED); ForwardM2(&hroboclaw_mc1, MAX_SPEED);
                    ForwardM1(&hroboclaw_mc2, MAX_SPEED); ForwardM2(&hroboclaw_mc2, MAX_SPEED);
                }
                else if (last_stable_key == '8') {
                    // 8 = BACKWARD
                    BackwardM1(&hroboclaw_mc1, MAX_SPEED); BackwardM2(&hroboclaw_mc1, MAX_SPEED);
                    BackwardM1(&hroboclaw_mc2, MAX_SPEED); BackwardM2(&hroboclaw_mc2, MAX_SPEED);
                }
                else if (last_stable_key == '4') {
                    // 4 = TANK LEFT (Left side back, Right side forward)
                    BackwardM1(&hroboclaw_mc1, MAX_SPEED); BackwardM2(&hroboclaw_mc1, MAX_SPEED);
                    ForwardM1(&hroboclaw_mc2, MAX_SPEED);  ForwardM2(&hroboclaw_mc2, MAX_SPEED);
                }
                else if (last_stable_key == '6') {
                    // 6 = TANK RIGHT (Left side forward, Right side back)
                    ForwardM1(&hroboclaw_mc1, MAX_SPEED);  ForwardM2(&hroboclaw_mc1, MAX_SPEED);
                    BackwardM1(&hroboclaw_mc2, MAX_SPEED); BackwardM2(&hroboclaw_mc2, MAX_SPEED);
                }
                else if (last_stable_key == '5') {
                    // 5 = WEAPON
                    ForwardM1(&hroboclaw_mc3, MAX_SPEED);
                }
                else {
                    // No key or released = STOP
                    Stop_All_Motors();
                }

                HAL_Delay(10); // Polling stability
            }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMA1_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE2 PE4 PE5 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PD3 PD4 PD5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
 * @brief  Scans 3x4 Keypad on PD3-6 (Rows) and PE2,4,5 (Cols)
 */
char Keypad_Scan(void) {
    uint16_t RowPins[] = {GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6};
    uint16_t ColPins[] = {GPIO_PIN_2, GPIO_PIN_4, GPIO_PIN_5};
    char map[4][3] = {{'1','2','3'},{'4','5','6'},{'7','8','9'},{'*','0','#'}};

    for (int r = 0; r < 4; r++) {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOD, RowPins[r], GPIO_PIN_SET);
        HAL_Delay(1);

        for (int c = 0; c < 3; c++) {
            if (HAL_GPIO_ReadPin(GPIOE, ColPins[c]) == GPIO_PIN_SET) return map[r][c];
        }
    }
    return 0;
}

/**
 * @brief  Initializes all three RoboClaws
 */
void RoboClaw_Init_All(void) {
    hroboclaw_mc1.hserial = hserial_uart5; hroboclaw_mc1.packetserial_address = LEFT_RC;
    hroboclaw_mc2.hserial = hserial_uart5; hroboclaw_mc2.packetserial_address = RIGHT_RC;
    hroboclaw_mc3.hserial = hserial_uart5; hroboclaw_mc3.packetserial_address = WEAPON_RC;

    roboClaw_init(&hroboclaw_mc1);
    roboClaw_init(&hroboclaw_mc2);
    roboClaw_init(&hroboclaw_mc3);
}

/**
 * @brief  Sends Stop command to all motors
 */
void Stop_All_Motors(void) {
    ForwardM1(&hroboclaw_mc1, 0); ForwardM2(&hroboclaw_mc1, 0);
    ForwardM1(&hroboclaw_mc2, 0); ForwardM2(&hroboclaw_mc2, 0);
    ForwardM1(&hroboclaw_mc3, 0);
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
