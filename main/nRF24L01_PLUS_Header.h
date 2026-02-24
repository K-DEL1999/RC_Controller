#ifndef NRF24L01_PLUS_HEADER_H
#define NRF24L01_PLUS_HEADER_H

#include "SPI_Driver_Header.h"

// Enums assign values to elements starting from 0 automatically -- So CONFIG = 0, EN_AA = 1 etc. 
// Since the registers range from 0x00 to 0x1D the enum automatically handles the assignments 
// However since 3 registers dont have available addresses we skip them a begin at the next 
//  valid register 1C. The enum then continues counting up from 0x1C. 
//
// 29 registers total but 3 have no register mapping --- ACK_PLD, TX_PLD, RX_PLD 
enum nrf_registers {
    CONFIG, 
    EN_AA, 
    EN_RXADDR,
    SETUP_AW,
    SETUP_RETR,
    RF_CH,
    RF_SETUP,
    STATUS,
    OBSERVE_TX,
    CD, 
    RX_ADDR_P0,
    RX_ADDR_P1,
    RX_ADDR_P2,
    RX_ADDR_P3,
    RX_ADDR_P4,
    RX_ADDR_P5,
    TX_ADDR,
    RX_PW_P0,
    RX_PW_P1,
    RX_PW_P2,
    RX_PW_P3,
    RX_PW_P4,
    RX_PW_P5,
    FIFO_STATUS,
    DYNPD = 0x1C,
    FEATURE 
};

// receiver_or_transmitter --- configure as primary transmitter or primary receiver
// module will receive and transmit but the module needs to know which device will be 
// predominantly transmitting and which will be predominantly receiving.
// receiver_or_transmitter --- 1:transmitter, 0:receiver 
void configure_transceiver(int receiver_or_transmitter); 

#endif /*NRF24L01_PLUS_HEADER_H*/
