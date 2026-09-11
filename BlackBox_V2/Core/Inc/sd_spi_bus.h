/*
 * sd_spi_bus.h
 *
 * CubeMX-safe SD SPI helpers (not regenerated). Used by user_diskio.c.
 */
#ifndef INC_SD_SPI_BUS_H_
#define INC_SD_SPI_BUS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TEMP: 1 = SD CS on PC4 (was PC12 / CS_SPI1). Set 0 to restore. */
#ifndef SD_CS_USE_PC4
#define SD_CS_USE_PC4 1
#endif

/* TEMP: ST-Link VCP USART2 @ 115200 SD SPI probe. Set 0 when done. */
#ifndef SD_SPI_DEBUG
#define SD_SPI_DEBUG 1
#endif

/* Init breadcrumbs (0 = success) */
extern volatile uint8_t sd_fail_stage; /* 1=M0 CMD0 2=M3 CMD0 3=ACMD41 4=CMD16 */
extern volatile uint8_t sd_last_r1;
extern volatile uint8_t sd_m0_r1;
extern volatile uint8_t sd_m3_r1;
extern volatile uint8_t sd_bus_idle; /* expected 0xFF with CS high */

/** Re-assert CS lines idle-high after Cube MX_GPIO_Init (survives regen). */
void SD_CS_ForceIdleHigh(void);

void SD_Select(void);
void SD_Deselect(void);
void SD_Dummy(void);
void SD_SPI_Reconfig(uint32_t baud, uint32_t cpol, uint32_t cpha);
void SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc);
void SD_ReadR7(uint8_t *response);
uint8_t SD_ReadR1(void);

void SD_SPI_DebugProbe(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_SD_SPI_BUS_H_ */
