/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
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

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include <stdbool.h>
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
static bool block_addressing = false;
/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

void SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc){ // Credit to Claude
    uint8_t frame[6];
    frame[0] = 0x40 | cmd;           // command byte: 0x40 OR'd with command number
    frame[1] = (uint8_t)(arg >> 24); // argument, MSB first
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg);
    frame[5] = crc;

    uint8_t rx;
    for (int i = 0; i < 6; i++){
        HAL_SPI_TransmitReceive(&hspi1, &frame[i], &rx, 1, HAL_MAX_DELAY);
    }
}

void SD_ReadR7(uint8_t *response){ // EXTENSION OF R1 BYTE
	uint8_t tx = 0xFF, rx = 0xFF;
	int timeout = 10;
	while (timeout--){
		HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
		if ((rx & 0x80) == 0) break;
	}
	response[0] = rx;

	for (int i = 1; i<5; i++){
		HAL_SPI_TransmitReceive(&hspi1,&tx, &rx, 1, HAL_MAX_DELAY);
		response[i]	= rx;
	}
}

uint8_t SD_ReadR1(void){
    uint8_t tx = 0xFF, rx = 0xFF;
    int timeout = 10;
    while (timeout--){
        HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
        if ((rx & 0x80) == 0) break;
    }
    return rx;
}
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */

)
{
  /* USER CODE BEGIN INIT */
	uint8_t dummy = 0xFF;
	uint8_t rx;
	uint32_t Timeout = 5000;
	uint32_t tries = 1000;


	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; // REDUCED SPI BAUD RATE FOR INIT PURPOSES
	HAL_SPI_Init(&hspi1);

		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);  // CS high
		for (int i = 0; i < 10; i++){ // 10 bytes  = 80 clocks, 80 > 74 bytes
			 HAL_SPI_TransmitReceive(&hspi1, &dummy, &rx, 1, HAL_MAX_DELAY);
		}
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);  // CS low
	SD_SendCommand(0, 0x00000000, 0x95); //SEND CMD 0 (IDLE STATE)
	// KNOWN CRC VALUE FOR CMD0

	 uint8_t response = SD_ReadR1();
	 if (response == 0x01){
		 Stat = 0;
	 } else {
		 Stat = STA_NOINIT;
	 }


	 SD_SendCommand(8,0x000001AA, 0x87); // SEND CMD8 FOR CARD VOLTAGE RANGE SUPPORT (SDHC/SDXC DETECTION)
	 // KNOWN CORRECT CRC VALUE FOR CMD8
	 uint8_t ReadR7[5];

	 SD_ReadR7(ReadR7);

	 bool v2_card = (ReadR7[4] == 0XAA); //0XAA RECOMMENDED TEST BYTE

	 bool sd_ready = false;
	 while (tries > 0 && !sd_ready){
		 SD_SendCommand(55, 0x00000000, 0x01); // SEND CMD55 TO CHECK IF CARD IS READY FOR DATA TRANSFER

	 	 if (v2_card){
	 		 SD_SendCommand(41,0x40000000, 0x01);
	 	 }
		 else{
			 SD_SendCommand(41,0x00000000, 0x01);
		 }
		 uint8_t response7 = SD_ReadR1();
		 if (response7 == 0x00){
			 sd_ready = true;
		 }
		 tries--;
			 Stat = sd_ready ? 0 : STA_NOINIT;

	}
		if (Stat == 0){
			SD_SendCommand(58, 0x00000000, 0x01); // SEND CMD58 TO DETERMINE ADDRESSING MODE OF CARD
			uint8_t ocr_response[5];
			SD_ReadR7(ocr_response);
			hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; // INCREASE SPI1 BAUD RATE TO 11.25 MBITS/S (original)
			HAL_SPI_Init(&hspi1); // UPDATE SPI1

	block_addressing = (ocr_response[1] & 0x40) != 0;
	 }
	 return Stat;
    /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    Stat = STA_NOINIT;
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */

	for (int s = 0; s < count; s++){
		uint32_t address = block_addressing ? sector : ((sector + s) * 512); // IF BLOCK ADDRESSING IS TRUE (SDHC/SDXC CONFRIMED)
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);  // CS low
		SD_SendCommand(17,address,0x01); // CALL CMD17
		//WAIT FOR DATA START TOKEN
		uint8_t tx = 0xFF, token = 0xFF;
		uint32_t start = HAL_GetTick();
		while (token != 0xFE){
			HAL_SPI_TransmitReceive(&hspi1, &tx, &token, 1, HAL_MAX_DELAY);
			if ((HAL_GetTick()-start) > 200){
				return RES_ERROR;
			}
		}
		//READ 512 BYTES
		uint8_t tx_dummy[512];
		for (int i = 0; i < 512; i++){
			tx_dummy[i] = 0xFF;
		}
		HAL_SPI_TransmitReceive (&hspi1, tx_dummy, buff + s * 512, 512, HAL_MAX_DELAY);

		//DISCARD 2 CRC BYTES
		uint8_t crc_dummy[2];
		uint8_t tx2[2] = {0xFF, 0XFF};
		HAL_SPI_TransmitReceive(&hspi1, tx2, crc_dummy, 2, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);  // CS high
	}
	return RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
  /* USER CODE HERE */
  	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); // CS LOW
	SD_SendCommand(24,address,0x01); // CMD 24

	uint8_t token = 0xFE;
	uint8_t rx_dummy;
	uint32_t rx_buffer_512[512];
	uint32_t address = block_addressing ? sector : ((sector + s) * 512);

	HAL_SPI_TransmitReceive(&hspi1, &token, &rx_dummy, 1, HAL_MAX_DELAY);
	HAL_SPI_TransmitReceive (&hspi1,(uint8_t*)buff, rx_buffer_512, 512, HAL_MAX_DELAY);

	uint8_t tx2[2] = {0xFF, 0XFF};
	uint8_t rx2[2];
	HAL_SPI_TransmitReceive(&hspi1, tx2,rx2,2, HAL_MAX_DELAY); // SEND DUMMY CRC BYTE IN EXCHANGE FOR DATA RESPONSE
	uint8_t tx = 0xFF, data_response; // CHECKING DATA RESPONSE TOKEN
	HAL_SPI_TransmitReceive (&hspi1, &tx, &data_response, 1, HAL_MAX_DELAY);
	if ((data_response & 0x1F) != 0x05){ // 0X05 = DATA ACCEPTED
		return RES_ERROR;
	}
	// ALLOW CARD TO FINISH WRITES
	uint8_t busy = 0x00;
	uint32_t start = HAL_GetTick();
	while (busy == 0x00){
		HAL_SPI_TransmitReceive(&hspi1, &tx, &busy, 1, HAL_MAX_DELAY);
		if ((HAL_GetTick() - start)> 500){ // WRITES TAKE LONGER THAN READS
			return RES_ERROR;
		}
	}
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);  // CS high
    return RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    DRESULT res = RES_ERROR;
    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

