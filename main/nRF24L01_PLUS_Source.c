#include "nRF24L01_PLUS_Header.h"

static unsigned char* r_register(unsigned char address);
static int w_register(unsigned char address, unsigned char bit_to_toggle, unsigned char turn_on_off);

void configure_transceiver(int receiver_or_transmitter){
    int err = w_register(CONFIG, 0, 0); // I want to turn 0th bit on 
 
    printf("\n");
    //%04X --- formatting (0 - padding done with 0s, 2 - atleast 2 characters wide, X - hexadecimal)
    //printf("0x%02X ", *reg_data);     
    (err) ? printf("\nWrite Successful!!") : printf("\nWrite NOT Successful!!\n") ;    
    printf("\n");
}

static unsigned char* r_register(unsigned char address){
    /*
        To read from a register you transmit the 5 bit register map address of the 
        register you wish to read from. All register addresses 
        are a byte so we cast the int enum values to 8 bit or 1 byte values. Then the 
        byte is shifted left 4 times and finally a 5 bit mask is applied to satisfy the 
        command requirements.

        2 bytes have to be sent - first byte is command and second bytes is dummy bits 
        to allow nRF module to send back data. 

        below is the line that performs the necessary adjustments to the reg addresses
    */
    unsigned char adjusted_address[2];
    adjusted_address[0] = address & 0x1F; // 0x1F applies the 5 bit mask 
    adjusted_address[1] = 0x00; // extra transmission for receiving 

    return spi_transmit(adjusted_address, 2, 1);
}

// Byte addresse bits are label 0 to 7 -- 0 being the LSB and 7 being the MSB
// So to change the 2nd bit (bit 0) bit_to_toggle would be 0
//    to change the 3rd bit (bit 2) bit_to_toggle would be 2
// turn_on_off --- 1 to turn on : 0 to turn off
static int w_register(unsigned char address, unsigned char bit_to_toggle, unsigned char turn_on_off){
    /*
        To write to a register you transmit the 5 bit register map address of the register
        with the 6th bit turned on.You then follow the command with 
        the desired data to be written. This function will only update 1 bit at a time.

        2 bytes will be transmitted -- 1 byte being the cmd and then next byte being the data

        below are the lines that prepare the data and command for transmission
    */
    // Get current register value at given address
    unsigned char cur_reg_val = *(r_register(address));
    
    unsigned char adjusted_address[2];
    adjusted_address[0] = (address & 0x1F) | 0x20; // 0x1F applies the 5 bit mask and (... | 0x20) turns on the 6th bit 
    adjusted_address[1] = (turn_on_off) ? (cur_reg_val | (0x01 << bit_to_toggle)) : (cur_reg_val & ~(0x01 << bit_to_toggle)); 
    spi_transmit(adjusted_address, 2, 0);

    // checks whether write was successful. Returns true if its successful otherwise returns false
    return (*(adjusted_address+1) == *(r_register(address))); 
}






