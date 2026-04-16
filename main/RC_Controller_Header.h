#ifndef RC_CONTROLLER_HEADER_H
#define RC_CONTROLLER_HEADER_H

#include "nRF24L01_PLUS_driver/nRF24L01_PLUS_Header.h"

#define DATA_SIZE PAYLOAD_BYTES

void init_rc_controller(void);

#if !RECEIVER_OR_TRANSMITTER
unsigned char * get_data(void);
#endif

#endif /* RC_CONTROLLER_HEADER_H 
*/
