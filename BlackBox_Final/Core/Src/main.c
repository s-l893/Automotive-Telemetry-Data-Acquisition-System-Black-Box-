/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "can.h"
#include "fatfs.h"
#include "i2c.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <string.h>
#include <stdio.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "fatfs.h"
#include "mpu6050.h"
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct{
	float lat;
	float lon;
	float speed_kph;
	uint8_t has_fix;
} GPS_Data;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
FATFS fs;
FIL fil;
FRESULT res;
UINT bw;
MPU6050_t myMPU;

volatile uint8_t system_running = 1;
char filename[20];
RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

// MPU6050
float Ax_offset = 0.0, Ay_offset = 0.0, Az_offset = 0.0;
float alpha = 0.5;
float Ax = 0.0, Ay = 0.0, Az_filt = 0.0;
float max_ax = 0.0, max_ay = 0.0;

MPU6050_t MPU6050;
GPS_Data currentGPS;
char gps_raw_buffer[120];
int buffer_idx = 0;

// CAN BUS
uint32_t rpm = 0;
float throttle = 0.0;
uint8_t vtec_on = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}
float nmea_to_dec(float nmea) {
    int deg = (int)(nmea / 100);
    float min = nmea - (deg * 100);
    return (float)deg + (min / 60.0f);
}

void process_gps_line(char *line) {
    if (strstr(line, "$GPRMC")) {
        char *ptr = line;
        int field = 0;
        char *token = strtok(ptr, ",");
        while (token != NULL) {
            field++;
            if (field == 3) currentGPS.has_fix = (*token == 'A');
            if (field == 4 && currentGPS.has_fix) currentGPS.lat = nmea_to_dec(atof(token));
            if (field == 6 && currentGPS.has_fix) currentGPS.lon = nmea_to_dec(atof(token));
            if (field == 7 && currentGPS.has_fix && *token == 'W') currentGPS.lon *= -1.0f; // London, ON Fix
            if (field == 8 && currentGPS.has_fix) currentGPS.speed_kph = atof(token) * 1.852f;
            token = strtok(NULL, ",");
        }
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

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
  MX_I2C1_Init();
  MX_FATFS_Init();
  MX_CAN1_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_UART4_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n=== Automotive Black Box V1.0 ===\r\n");
  printf("Initializing...\r\n\r\n");

  // ============================================================================
  // CAN INITIALIZATION
  // ============================================================================
  printf("CAN Setup:\r\n");
  printf("  APB1 Clock: %lu Hz\r\n", HAL_RCC_GetPCLK1Freq());

  // Configure CAN filter to accept all messages
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK) {
      printf("  ERROR: CAN filter failed\r\n");
      Error_Handler();
  }

  // Start CAN peripheral
  if (HAL_CAN_Start(&hcan1) != HAL_OK) {
      printf("  WARNING: CAN start failed\r\n");
      printf("  This is OK - will retry when car is connected\r\n");
  } else {
      printf("  CAN ready for car connection\r\n");
  }
  printf("\r\n");

  // ============================================================================
  // MPU6050 & OLED INITIALIZATION
  // ============================================================================
  printf("Sensors:\r\n");
  MPU6050_Init(&hi2c1);
  printf("  MPU6050: OK\r\n");

  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString("BLACK BOX V1.0", Font_7x10, White);
  ssd1306_SetCursor(2, 12);
  ssd1306_WriteString("CALIBRATING...", Font_7x10, White);
  ssd1306_UpdateScreen();

  // ============================================================================
  // ACCELEROMETER CALIBRATION
  // ============================================================================
  printf("  Calibrating accelerometer...\r\n");
  float sumX = 0, sumY = 0, sumZ = 0;
  int samples = 100;

  for (int i = 0; i < samples; i++) {
      MPU6050_Read_Accel(&hi2c1, &myMPU);
      sumX += myMPU.Ax;
      sumY += myMPU.Ay;
      sumZ += (myMPU.Az - 1.0f);
      HAL_Delay(10);
  }

  Ax_offset = sumX / samples;
  Ay_offset = sumY / samples;
  Az_offset = sumZ / samples;
  printf("  Calibration complete (offsets: %.2f, %.2f, %.2f)\r\n\r\n",
         Ax_offset, Ay_offset, Az_offset);

  ssd1306_Fill(Black);
  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString("BLACK BOX", Font_7x10, White);
  ssd1306_SetCursor(2, 12);
  ssd1306_WriteString("READY", Font_11x18, White);
  ssd1306_UpdateScreen();
  HAL_Delay(1000);

  // ============================================================================
  // GPS INITIALIZATION
  // ============================================================================
  memset(&currentGPS, 0, sizeof(currentGPS));
  printf("GPS: Waiting for fix...\r\n\r\n");

  // ============================================================================
  // RTC & FILENAME SETUP
  // ============================================================================
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  sprintf(filename, "%02d%02d%02d%02d.csv",
          sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes);

  // ============================================================================
  // SD CARD INITIALIZATION
  // ============================================================================
  printf("Storage:\r\n");
  printf("  Filename: %s\r\n", filename);

  HAL_Delay(500);
  res = f_mount(&fs, "", 1);

  if (res == FR_OK) {
      if (f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
          f_puts("Time,Ax,Ay,Az,Lat,Lon,Spd,RPM,Throttle,VTEC\n", &fil);
          f_close(&fil);
          printf("  SD Card: Ready\r\n");
      } else {
          printf("  SD Card: File creation failed\r\n");
      }
  } else {
      printf("  SD Card: Mount failed (error %d)\r\n", res);
  }

  printf("\r\n=== System Ready ===\r\n");
  printf("Connect to car OBD port and start engine\r\n");
  printf("Monitoring CAN bus for messages...\r\n\r\n");

  // Clear OLED for runtime display
  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();

  /* USER CODE END 2 */


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (system_running) {

      // 1. HEARTBEAT LED
      static uint32_t last_blink = 0;
      if (HAL_GetTick() - last_blink >= 250) {
          last_blink = HAL_GetTick();
          HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
      }

      // 2. CAN RECEIVE - Check for messages from car
      if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0) {
          CAN_RxHeaderTypeDef rxHeader;
          uint8_t rxData[8];

          if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
              // Print all received messages for debugging
              printf("CAN RX: ID=0x%03lX Data=", rxHeader.StdId);
              for (int i = 0; i < rxHeader.DLC; i++) {
                  printf("%02X ", rxData[i]);
              }
              printf("\r\n");

              // Parse Honda RPM (ID 0x158, bytes 2-3)
              if (rxHeader.StdId == 0x158) {
                  rpm = (rxData[2] << 8) | rxData[3];
                  printf("  -> RPM: %lu\r\n", rpm);
              }

              // Parse standard OBD-II RPM response (ID 0x7E8)
              if (rxHeader.StdId == 0x7E8 && rxData[1] == 0x41 && rxData[2] == 0x0C) {
                  rpm = ((rxData[3] << 8) | rxData[4]) / 4;
                  printf("  -> RPM (OBD): %lu\r\n", rpm);
              }
          }
      }

      // 3. ACCELEROMETER - Read G-forces
      static uint32_t last_accel = 0;
      if (HAL_GetTick() - last_accel >= 20) {
          last_accel = HAL_GetTick();

          MPU6050_Read_Accel(&hi2c1, &myMPU);
          Ax = myMPU.Ax - Ax_offset;
          Ay = myMPU.Ay - Ay_offset;
          Az_filt = myMPU.Az - Az_offset;
      }

      // 4. GPS - Read location data
      uint8_t gps_byte;
      if (HAL_UART_Receive(&huart4, &gps_byte, 1, 0) == HAL_OK) {
          if (gps_byte == '\n') {
              gps_raw_buffer[buffer_idx] = '\0';
              process_gps_line(gps_raw_buffer);
              buffer_idx = 0;
          } else if (buffer_idx < sizeof(gps_raw_buffer) - 1) {
              gps_raw_buffer[buffer_idx++] = gps_byte;
          }
      }

      // 5. SD CARD LOGGING - Save data every 500ms
      static uint32_t last_log = 0;
      if (HAL_GetTick() - last_log >= 500) {
          last_log = HAL_GetTick();

          if (f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE) == FR_OK) {
              char buf[128];
              int len = sprintf(buf, "%lu,%.2f,%.2f,%.2f,%.6f,%.6f,%.1f,%lu,%.1f,%d\n",
                              HAL_GetTick(), Ax, Ay, Az_filt,
                              currentGPS.lat, currentGPS.lon, currentGPS.speed_kph,
                              rpm, throttle, vtec_on);
              f_write(&fil, buf, len, &bw);
              f_close(&fil);
          }
      }

      // 6. OLED DISPLAY - Update screen every 200ms
      static uint32_t last_oled = 0;
      if (HAL_GetTick() - last_oled >= 200) {
          last_oled = HAL_GetTick();

          ssd1306_Fill(Black);
          char display_buf[20];

          // Show RPM
          sprintf(display_buf, "RPM:%lu", rpm);
          ssd1306_SetCursor(2, 2);
          ssd1306_WriteString(display_buf, Font_7x10, White);

          // Show max G-force
          float max_g = (fabs(Ax) > fabs(Ay)) ? Ax : Ay;
          sprintf(display_buf, "G: %.2f", max_g);
          ssd1306_SetCursor(2, 15);
          ssd1306_WriteString(display_buf, Font_11x18, White);

          // Show GPS status
          if (currentGPS.has_fix) {
              sprintf(display_buf, "GPS:OK %.0f", currentGPS.speed_kph);
          } else {
              sprintf(display_buf, "GPS:--");
          }
          ssd1306_SetCursor(2, 40);
          ssd1306_WriteString(display_buf, Font_7x10, White);

          ssd1306_UpdateScreen();
      }

  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void){


  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_13) // Check if the interrupt came from the Blue Button
  {
    system_running = 0; // This flips the flag to break your main while() loop
  }
}

/* USER CODE END 4 */

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
#ifdef USE_FULL_ASSERT
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
