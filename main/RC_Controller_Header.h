#ifndef RC_CONTROLLER_HEADER_H
#define RC_CONTROLLER_HEADER_H

#include "nRF24L01_PLUS_Header.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CE 27

// 0 for receiver --- 1 for transmitter
#define RECEIVER_OR_TRANSMITTER 1 

void init_rc_controller(void);

#endif /* RC_CONTROLLER_HEADER_H 
*/
