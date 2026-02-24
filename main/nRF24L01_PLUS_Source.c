#include "nRF24L01_PLUS_Header.h"

static unsigned char* r_register(unsigned char address);

void configure_transceiver(int receiver_or_transmitter){
    unsigned char* reg_data = r_register(EN_AA);
    
    printf("\n");
    //%04X --- formatting (0 - padding done with 0s, 2 - atleast 2 characters wide, X - hexadecimal)
    printf("0x%02X ", *reg_data);     
    printf("\n");
}

static unsigned char* r_register(unsigned char address){
    /*
        To read from a register you transmit the 5 bit register map address of the 
        register you wish to read from. All register addresses are a byte so we cast 
        the int enum values to 8 bit or 1 byte values. Then the byte is shifted left 
        4 times and finally a 5 bit mask is applied to satisfy the command requirements.

        2 bytes have to be sent - first byte is command and second bytes is dummy bits 
        to allow nRF module to send back data. 

        below is the line that performs the necessary adjustments to the reg addresses
    */
    unsigned char adjusted_address[2];
    adjusted_address[0] = address & 0x1F; // 0x1F applies the 5 bit mask 
    adjusted_address[1] = 0x00; // extra transmission for receiving 

    return spi_transmit(adjusted_address, 2, 1);
}
