#include "RC_Controller_Header.h"

void init_rc_controller(void){
    init_spi_driver();
    configure_transceiver(RECEIVER_OR_TRANSMITTER); 
}


