/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    user_diskio.c
  * @brief   ROBUST / DEBUG VERSION
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
#include <stdio.h>       // Added for printf debugging
#include "ff_gen_drv.h"
#include "spi.h"

/* Definitions for MMC/SDC command */
#define CMD0     (0x40+0)     /* GO_IDLE_STATE */
#define CMD1     (0x40+1)     /* SEND_OP_COND */
#define CMD8     (0x40+8)     /* SEND_IF_COND */
#define CMD9     (0x40+9)     /* SEND_CSD */
#define CMD10    (0x40+10)    /* SEND_CID */
#define CMD12    (0x40+12)    /* STOP_TRANSMISSION */
#define CMD16    (0x40+16)    /* SET_BLOCKLEN */
#define CMD17    (0x40+17)    /* READ_SINGLE_BLOCK */
#define CMD18    (0x40+18)    /* READ_MULTIPLE_BLOCK */
#define CMD23    (0x40+23)    /* SET_BLOCK_COUNT */
#define CMD24    (0x40+24)    /* WRITE_BLOCK */
#define CMD25    (0x40+25)    /* WRITE_MULTIPLE_BLOCK */
#define CMD41    (0x40+41)    /* SEND_OP_COND (ACMD) */
#define CMD55    (0x40+55)    /* APP_CMD */
#define CMD58    (0x40+58)    /* READ_OCR */

/* CS Pin definitions */
#define SD_CS_PORT GPIOA
#define SD_CS_PIN  GPIO_PIN_4

extern SPI_HandleTypeDef hspi1;
volatile DSTATUS Stat = STA_NOINIT;
uint8_t CardType; /* Card type flags */

// --- SPI Helper Functions ---

static uint8_t SPI_RxByte(void) {
    uint8_t dummy = 0xFF;
    uint8_t data;
    HAL_SPI_TransmitReceive(&hspi1, &dummy, &data, 1, 100);
    return data;
}

static void SPI_TxByte(uint8_t data) {
    HAL_SPI_Transmit(&hspi1, &data, 1, 100);
}

static void SELECT(void) {
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
}

static void DESELECT(void) {
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    SPI_RxByte(); // Dummy clock
}

static uint8_t wait_ready(void) {
    uint8_t res;
    uint32_t timer = HAL_GetTick();
    SPI_RxByte();
    do {
        res = SPI_RxByte();
        if (res == 0xFF) return 1;
    } while ((HAL_GetTick() - timer) < 500);
    return 0;
}

static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t n, res;
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) return res;
    }
    DESELECT();
    SELECT();

    // AGGRESSIVE FIX: Do not wait for ready if sending CMD0 (Reset)
    // The card might not be in SPI mode yet, so waiting checks the wrong pin state.
    if (cmd != CMD0) {
        if (wait_ready() == 0) return 0xFF;
    }

    SPI_TxByte(cmd);
    SPI_TxByte((uint8_t)(arg >> 24));
    SPI_TxByte((uint8_t)(arg >> 16));
    SPI_TxByte((uint8_t)(arg >> 8));
    SPI_TxByte((uint8_t)arg);

    uint8_t crc = 0x01;
    if (cmd == CMD0) crc = 0x95;
    if (cmd == CMD8) crc = 0x87;
    SPI_TxByte(crc);

    if (cmd == CMD12) SPI_RxByte();

    // TIMEOUT FIX: Increased to 200 checks
    n = 200;
    do res = SPI_RxByte(); while ((res & 0x80) && --n);
    return res;
}

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
	Stat = STA_NOINIT;

	  // 1. Power up sequence: Send 10 dummy clocks with CS HIGH
	  DESELECT();
	  for (uint8_t i = 0; i < 10; i++) SPI_TxByte(0xFF);

	  // 2. Put card in SPI Mode (CMD0)
	  if (send_cmd(CMD0, 0) == 1) {
	      // 3. Check interface condition (CMD8)
	      if (send_cmd(CMD8, 0x1AA) == 1) {
	          // 4. Initialize card (ACMD41)
	          uint32_t timeout = HAL_GetTick() + 1000;
	          while (send_cmd(CMD41 | 0x80, 0x40000000) && (HAL_GetTick() < timeout));

	          if (HAL_GetTick() < timeout) Stat &= ~STA_NOINIT; // SUCCESS!
	      }
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
	return Stat; // Returns 0 (Ready) if initialization succeeded
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
	if (Stat & STA_NOINIT) return RES_NOTRDY;

	  if (send_cmd(CMD17, sector) == 0) { // Read single block
	      // Wait for data token 0xFE
	      uint16_t timeout = 1000;
	      while (SPI_RxByte() != 0xFE && --timeout);

	      for (uint16_t i = 0; i < 512; i++) *buff++ = SPI_RxByte();

	      SPI_RxByte(); SPI_RxByte(); // Discard CRC
	      DESELECT();
	      return RES_OK;
	  }
	  return RES_ERROR;
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
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  // CMD24 = Write Single Block
  if (send_cmd(CMD24, sector) == 0) {
      SPI_TxByte(0xFF); // Dummy clock
      SPI_TxByte(0xFE); // Data Token (tells SD card "here comes data")

      for (uint16_t i = 0; i < 512; i++) SPI_TxByte(*buff++); // Send 512 byte sector

      SPI_TxByte(0xFF); SPI_TxByte(0xFF); // Dummy CRC

      // Check if card accepted the data (0x05 = Data Accepted)
      if ((SPI_RxByte() & 0x1F) == 0x05) {
          while (SPI_RxByte() == 0x00); // Wait while card is "Busy" writing to flash
          DESELECT();
          return RES_OK;
      }
  }
  DESELECT();
  return RES_ERROR;
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
	if (Stat & STA_NOINIT) return RES_NOTRDY;

	    switch (cmd) {
	        case CTRL_SYNC:
	            SELECT();
	            if (wait_ready() == 1) { // Wait for the card to finish internal programming
	                DESELECT();
	                return RES_OK;
	            }
	            DESELECT();
	            return RES_ERROR;

	        case GET_SECTOR_COUNT:
	            // Most libraries need this to calculate total space
	            *(DWORD*)buff = 1000000; // Placeholder for ~512MB
	            return RES_OK;

	        case GET_SECTOR_SIZE:
	            *(WORD*)buff = 512;
	            return RES_OK;

	        case GET_BLOCK_SIZE:
	            *(DWORD*)buff = 1;
	            return RES_OK;
	    }
	    return RES_ERROR;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

