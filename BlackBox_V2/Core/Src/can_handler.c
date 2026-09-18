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

CAN_FilterTypeDef filter_config;

volatile CAN_RxHeaderTypeDef rx_header;
volatile uint8_t rx_data[8];
static can_frame_t can_storage[32];
volatile can_ring_buffer_t can_rb;
volatile uint32_t last_can_frame = 0;
volatile bool can_frame_received_flag = false;
volatile bool can_busoff_flag = false;


void can_handler_init(void){
    CAN_FilterTypeDef filter_config = {0};
    // HARDWARE BLOCKING CAN LIST FILTERS (NOT MASKING)
 	// BANK 0
    filter_config.FilterBank           = 0;
    filter_config.FilterMode           = CAN_FILTERMODE_IDLIST;
    filter_config.FilterScale          = CAN_FILTERSCALE_16BIT;
    filter_config.FilterIdHigh         = (0x158 << 5); // RPM
    filter_config.FilterIdLow          = (0x17C << 5); // Throttle
    filter_config.FilterMaskIdHigh     = (0x1A4 << 5); // Shifter/Gear
    filter_config.FilterMaskIdLow      = (0x1A6 << 5); // VCM
    filter_config.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter_config.FilterActivation     = CAN_FILTER_ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &filter_config);

    // Bank 1: ECT/TransTemp ID ONLY ONE ID REPEATED 4x TO FILL BANK
    filter_config.FilterBank       = 1;
    filter_config.FilterIdHigh     = (0x324 << 5);
    filter_config.FilterIdLow      = (0x324 << 5);
    filter_config.FilterMaskIdHigh = (0x324 << 5);
    filter_config.FilterMaskIdLow  = (0x324 << 5);

	HAL_CAN_ConfigFilter(&hcan1, &filter_config);
	HAL_CAN_Start(&hcan1);
	HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
	HAL_CAN_ActivateNotification(&hcan1, CAN_IT_ERROR | CAN_IT_BUSOFF); // ACTIVATING ERROR NOTIFICATION
	CANRingBuffer_Init(&can_rb, 32, can_storage);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){

	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, (uint8_t*)rx_data) == HAL_OK) {
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

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan){
	uint32_t error_code = HAL_CAN_GetError(hcan);
	if (error_code & HAL_CAN_ERROR_BOF){
		can_busoff_flag = true;
		fault_flags.can_fault = true;
	}
}
void CAN_Handler_RecoverBusOff(void){
	if (can_busoff_flag){
		HAL_CAN_Stop(&hcan1);                              // parameter: CAN handle
		HAL_CAN_Init(&hcan1);                              // parameter: CAN handle - reinitializes, clears bus-off
		HAL_CAN_ConfigFilter(&hcan1, &filter_config);       // REAPPLY FILTER
		HAL_CAN_Start(&hcan1);                              // parameter: CAN handle
		HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF); // re-activate both
		fault_flags.can_fault = false;
		can_busoff_flag = false; 							// CLEAR FLAGS
	}

}
