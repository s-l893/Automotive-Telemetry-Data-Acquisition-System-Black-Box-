/*
 * can_handler.c
 *
 *  Created on: Jul 9, 2026
 *      Author: Sunny Lin
 */


#include "can.h"
#include <string.h>
#include "can_ring_buffer.h"
#include <stdbool.h>
#include "fault.h"
#include "can_decode.h"

CAN_FilterTypeDef filter_config;

volatile CAN_RxHeaderTypeDef rx_header;
volatile uint8_t rx_data[8];
static can_frame_t can_storage[32];
volatile can_ring_buffer_t can_rb;
volatile uint32_t last_can_frame = 0;
volatile bool can_frame_received_flag = false;
volatile bool can_busoff_flag = false;

static void can_apply_filters(void)
{
	/* Hardware ID-list (not mask). Bank 0: four PIDs. Bank 1: 0x324 x4. */
	filter_config.FilterBank           = 0;
	filter_config.FilterMode           = CAN_FILTERMODE_IDLIST;
	filter_config.FilterScale          = CAN_FILTERSCALE_16BIT;
	filter_config.FilterIdHigh         = (0x158 << 5); /* RPM */
	filter_config.FilterIdLow          = (0x17C << 5); /* Throttle */
	filter_config.FilterMaskIdHigh     = (0x188 << 5); /* Gear / shifter */
	filter_config.FilterMaskIdLow      = (0x1A6 << 5); /* VCM */
	filter_config.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	filter_config.FilterActivation     = ENABLE;
	filter_config.SlaveStartFilterBank = 14;
	HAL_CAN_ConfigFilter(&hcan1, &filter_config);

	filter_config.FilterBank       = 1;
	filter_config.FilterIdHigh     = (0x324 << 5); /* ECT / ATF */
	filter_config.FilterIdLow      = (0x324 << 5);
	filter_config.FilterMaskIdHigh = (0x324 << 5);
	filter_config.FilterMaskIdLow  = (0x324 << 5);
	HAL_CAN_ConfigFilter(&hcan1, &filter_config);
}

void can_handler_init(void)
{
	can_apply_filters();

	HAL_CAN_Start(&hcan1);
	HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
	HAL_CAN_ActivateNotification(&hcan1, CAN_IT_ERROR | CAN_IT_BUSOFF);
	CANRingBuffer_Init(&can_rb, 32, can_storage);
}
// CAN RX intterupt handler
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, (uint8_t *)rx_data) == HAL_OK) {
		can_frame_t frame;
		// assign data obtained from CAN HAL call to frame struct
		frame.id = rx_header.StdId;
		frame.dlc = rx_header.DLC;
		memcpy(frame.data, (const void *)rx_data, sizeof(rx_data));
		// Timestamps recorded for can bus silence detection
		frame.timestamp = HAL_GetTick();
		last_can_frame = HAL_GetTick();
		can_frame_received_flag = true;
		CANRingBuffer_Push(&can_rb, frame); // push frame struct containing data to rb
	}
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	uint32_t error_code = HAL_CAN_GetError(hcan);
	if (error_code & HAL_CAN_ERROR_BOF) {
		can_busoff_flag = true;
		fault_flags.can_fault = true;
	}
}

void CAN_Handler_RecoverBusOff(void)
{
	if (!can_busoff_flag) {
		return;
	}

	HAL_CAN_Stop(&hcan1);
	hcan1.Init.Mode = CAN_MODE_SILENT;
	HAL_CAN_Init(&hcan1);
	can_apply_filters();
	HAL_CAN_Start(&hcan1);
	HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF);
	fault_flags.can_fault = false;
	can_busoff_flag = false;
}
