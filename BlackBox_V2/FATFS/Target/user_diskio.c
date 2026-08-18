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
#include "spi.h"
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

static void SD_CS_High(void)
{
	HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_SET); // CS high (PC12)
}

static void SD_CS_Low(void)
{
	HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_RESET); // CS low
}

/* Release DO: CS high + one dummy clock so card releases MISO */
static void SD_Deselect(void)
{
	uint8_t dummy = 0xFF;
	uint8_t rx;
	SD_CS_High();
	HAL_SPI_TransmitReceive(&hspi1, &dummy, &rx, 1, HAL_MAX_DELAY);
}

static void SD_Select(void)
{
	SD_CS_Low();
}

/* 8 clocks with CS unchanged — lets the card align to a byte boundary */
static void SD_Dummy(void)
{
	uint8_t tx = 0xFF, rx;
	HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
}

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
	int timeout = 1000;
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
    int timeout = 1000; // was 10 — too short for some cards after power-up
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
	uint8_t response = 0xFF;
	int cmd0_tries;

	(void)pdrv;
	Stat = STA_NOINIT;
	block_addressing = false;

	HAL_Delay(10);

	/*
	 * This module: Mode0 CMD0 enters SPI mode (R1 may be mis-sampled);
	 * Mode3 is used afterward for reliable R1 and the rest of init/transfers.
	 */
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	HAL_SPI_Init(&hspi1);

	SD_CS_High();
	for (int i = 0; i < 10; i++){
		HAL_SPI_TransmitReceive(&hspi1, &dummy, &rx, 1, HAL_MAX_DELAY);
	}

	for (cmd0_tries = 0; cmd0_tries < 10; cmd0_tries++){
		SD_Select();
		SD_Dummy();
		SD_SendCommand(0, 0x00000000, 0x95);
		response = SD_ReadR1();
		SD_Deselect();
		if (response != 0xFF){
			break;
		}
	}
	if (response == 0xFF){
		return Stat;
	}

	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
	hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	HAL_SPI_Init(&hspi1);

	SD_CS_High();
	for (int i = 0; i < 10; i++){
		HAL_SPI_TransmitReceive(&hspi1, &dummy, &rx, 1, HAL_MAX_DELAY);
	}

	response = 0xFF;
	for (cmd0_tries = 0; cmd0_tries < 10; cmd0_tries++){
		SD_Select();
		SD_Dummy();
		SD_SendCommand(0, 0x00000000, 0x95);
		response = SD_ReadR1();
		SD_Deselect();
		if (response == 0x01){
			break;
		}
	}
	if (response != 0x01){
		return Stat;
	}

	SD_Select();
	SD_Dummy();
	SD_SendCommand(8, 0x000001AA, 0x87);
	uint8_t ReadR7[5];
	SD_ReadR7(ReadR7);
	SD_Deselect();

	bool v2_card = (ReadR7[0] == 0x01) && (ReadR7[4] == 0xAA);

	/*
	 * ACMD41: release CS between CMD55 and ACMD41, dummy clock after each
	 * select, and 10ms poll spacing. Holding CS low across both commands
	 * mis-sampled R1 as 0x3F and wedged this module.
	 */
	bool sd_ready = false;
	uint32_t iter;

	for (iter = 0; iter < 100 && !sd_ready; iter++){
		SD_Select();
		SD_Dummy();
		SD_SendCommand(55, 0x00000000, 0x65);
		response = SD_ReadR1();
		SD_Deselect();

		if (response > 0x01){
			HAL_Delay(10);
			continue;
		}

		SD_Select();
		SD_Dummy();
		SD_SendCommand(41, v2_card ? 0x40000000 : 0x00000000, 0x77);
		response = SD_ReadR1();
		SD_Deselect();

		if (response == 0x00){
			sd_ready = true;
			break;
		}
		if (response != 0x01){
			break;
		}
		HAL_Delay(10);
	}

	if (!sd_ready){
		return Stat;
	}

	SD_Select();
	SD_Dummy();
	SD_SendCommand(58, 0x00000000, 0x01);
	uint8_t ocr_response[5];
	SD_ReadR7(ocr_response);
	SD_Deselect();

	block_addressing = (ocr_response[1] & 0x40) != 0;

	if (!block_addressing){
		SD_Select();
		SD_Dummy();
		SD_SendCommand(16, 512, 0x01);
		response = SD_ReadR1();
		SD_Deselect();
		if (response != 0x00){
			return Stat;
		}
	}

	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	HAL_SPI_Init(&hspi1);

	Stat = 0;
	SD_Deselect();
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
	(void)pdrv;
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
	(void)pdrv;

	for (int s = 0; s < count; s++){
		uint32_t address = block_addressing ? (sector + s): ((sector + s) * 512);

		SD_Select();
		SD_Dummy();
		SD_SendCommand(17, address, 0x01);
		if (SD_ReadR1() != 0x00){
			SD_Deselect();
			return RES_ERROR;
		}

		uint8_t tx = 0xFF, token = 0xFF;
		uint32_t start = HAL_GetTick();
		while (token != 0xFE){
			HAL_SPI_TransmitReceive(&hspi1, &tx, &token, 1, HAL_MAX_DELAY);
			if ((HAL_GetTick()-start) > 200){
				SD_Deselect();
				return RES_ERROR;
			}
		}

		uint8_t tx_dummy[512];
		for (int i = 0; i < 512; i++){
			tx_dummy[i] = 0xFF;
		}
		HAL_SPI_TransmitReceive(&hspi1, tx_dummy, buff + s * 512, 512, HAL_MAX_DELAY);

		uint8_t tx2[2] = {0xFF, 0XFF};
		uint8_t crc_dummy[2];
		HAL_SPI_TransmitReceive(&hspi1, tx2, crc_dummy, 2, HAL_MAX_DELAY);
		SD_Deselect();
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
	(void)pdrv;

	for (int s = 0; s <count; s++){
		uint32_t address = block_addressing ? (sector + s): ((sector + s) * 512);

		SD_Select(); // CS LOW
		SD_SendCommand(24, address, 0x01); // CMD24
		if (SD_ReadR1() != 0x00){ // wait for R1 before sending data token
			SD_Deselect();
			return RES_ERROR;
		}

		uint8_t token = 0xFE;
		uint8_t rx_dummy;
		uint8_t rx_buffer_512[512];

		HAL_SPI_TransmitReceive(&hspi1, &token, &rx_dummy, 1, HAL_MAX_DELAY); // SENDING DATA START TOKEN
		HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)(buff + s * 512), rx_buffer_512, 512, HAL_MAX_DELAY); // SEND ACTUAL DATA

		uint8_t tx2[2] = {0xFF, 0XFF};
		uint8_t rx2[2];
		HAL_SPI_TransmitReceive(&hspi1, tx2, rx2, 2, HAL_MAX_DELAY); // SEND DUMMY CRC BYTE IN EXCHANGE FOR DATA RESPONSE
		uint8_t tx = 0xFF, data_response; // CHECKING DATA RESPONSE TOKEN
		HAL_SPI_TransmitReceive(&hspi1, &tx, &data_response, 1, HAL_MAX_DELAY);
		if ((data_response & 0x1F) != 0x05){ // 0X05 = DATA ACCEPTED
			SD_Deselect();
			return RES_ERROR;
		}
		// ALLOW CARD TO FINISH WRITES (DO low while busy)
		uint8_t busy = 0x00;
		uint32_t start = HAL_GetTick();
		while (busy == 0x00){
			HAL_SPI_TransmitReceive(&hspi1, &tx, &busy, 1, HAL_MAX_DELAY);
			if ((HAL_GetTick() - start) > 500){ // WRITES TAKE LONGER THAN READS
				SD_Deselect();
				return RES_ERROR;
			}
		}
		SD_Deselect(); // CS high
	}
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
	    (void)pdrv;
	    switch (cmd){
	        case CTRL_SYNC:
	            return RES_OK;
	        case GET_SECTOR_SIZE:
	            *(WORD*)buff = 512;
	            return RES_OK;
	        case GET_BLOCK_SIZE:
	            *(DWORD*)buff = 1;
	            return RES_OK;
	        default:
	            return RES_PARERR;
	    }
	}
  /* USER CODE END IOCTL */

#endif /* _USE_IOCTL == 1 */
