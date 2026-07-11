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

CAN_FilterTypeDef filter_config;

volatile CAN_RxHeaderTypeDef rx_header;
volatile uint8_t rx_data[8];
static can_frame_t can_storage[32];
volatile can_ring_buffer_t can_rb;
volatile uint32_t last_can_frame = 0;
volatile bool can_frame_received_flag = false;


void can_handler_init(void){
	filter_config.FilterBank = 0;
	filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
	filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
	filter_config.FilterIdHigh = 0x0000;
	filter_config.FilterIdLow = 0x0000;
	filter_config.FilterMaskIdHigh = 0x0000;
	filter_config.FilterMaskIdLow = 0x0000;
	filter_config.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	filter_config.FilterActivation = ENABLE;

	HAL_CAN_ConfigFilter(&hcan1, &filter_config);
	HAL_CAN_Start(&hcan1);
	HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
	CANRingBuffer_Init(&can_rb, 32, can_storage);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){

	// pull message out of FIFO0
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, (uint8_t*)rx_data) == HAL_OK) {
	    // build can_frame_t here using rx_header.StdId, rx_header.DLC, rx_data[]
		can_frame_t frame;
		frame.id = rx_header.StdId;
		frame.dlc = rx_header.DLC;
		memcpy(frame.data, rx_data, sizeof(rx_data));
		frame.timestamp = HAL_GetTick();
		last_can_frame = HAL_GetTick();
		can_frame_received_flag = true;
		CANRingBuffer_Push(&can_rb, frame);
	}
}
