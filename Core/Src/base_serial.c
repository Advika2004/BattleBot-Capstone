/*
 * base_serial.c
 *
 *  Created on: Aug 30, 2018
 *      Author: sadeesh
 */

#include "base_serial.h"

// Define the actual variables (without 'extern')
#ifdef USE_USART5
SERIAL_HandleTypeDef* USART5_Serial_Handler;
ring_buffer_t USART5_Buffer_TX, USART5_Buffer_RX;
uint8_t USART5_HAL_Reg_Tx, USART5_HAL_Reg_Rx, USART5_Application_Reg_Tx;
#endif

#ifdef USE_USART3
SERIAL_HandleTypeDef* USART3_Serial_Handler;
ring_buffer_t USART3_Buffer_TX, USART3_Buffer_RX;
uint8_t USART3_HAL_Reg_Tx, USART3_HAL_Reg_Rx, USART3_Application_Reg_Tx;
#endif

SERIAL_HandleTypeDef* get_serial_handler(UART_HandleTypeDef *huartx){

	SERIAL_HandleTypeDef* ret_serial_handler = NULL;

#ifdef USE_USART5
	if(huartx->Instance == UART5){
		ret_serial_handler = USART5_Serial_Handler;
	}
#endif
#ifdef USE_USART3
	if(huartx->Instance == USART3){
		ret_serial_handler = USART3_Serial_Handler;
	}
#endif

	return ret_serial_handler;
}

