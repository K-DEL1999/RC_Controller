#include "nRF24L01_PLUS_Header.h"
#include "SPI/SPI_Driver_Header.h"
#include <stdio.h>
#include <unistd.h>
// usleep suspends the execution of the calling thread allowing other threads to ulitilize the 
//  CPU

static unsigned char nrf_tbuffer[NRF_TBUFFER_SIZE];
static unsigned char nrf_rbuffer[NRF_RBUFFER_SIZE];

/*
    unsigned char is used instead of char because some machines by default make char signed
    which can cause issues when casting from char to int or int to char because of the sign
    extension.
*/

// All size references are in bytes -- how many unsigned char elements there are 
static unsigned char* r_register(unsigned char address, unsigned char return_size); 
static void w_register(unsigned char address, unsigned char* data, unsigned char size);
static void set_address(void);
static void reset_address(void);

// CONFIG value will be needed for transmissions
//  saving its stat will eliminate the need for 
//  multiple reads!
static unsigned char config_reg; 
static unsigned char status_reg; // 0000 0000 <--- no flags 

#if RECEIVER_OR_TRANSMITTER
// Transmitter
static void set_as_ptx(void);
static void high_pulse_ptx(void);
static void w_tx_payload(unsigned char* data, unsigned char size);
static void transmit_payload(void);
static void flush_tx(void);
#else
// Receiver
static void set_as_prx(void);
static void high_pulse_prx(void);
static void receive_payload(void);
static unsigned char* r_rx_payload(void);
static void flush_rx(void);
#endif

void configure_transceiver(void){
    init_spi_driver();
    
    // set as primary transmitter or primary receiver
    #if RECEIVER_OR_TRANSMITTER
    set_as_ptx();
    //(*r_register(CONFIG, 1) == 0x09) ? printf("SUCCESS!! Set as PTX\n") : printf("FAILED!!\n");
    #else
    set_as_prx();
    //(*r_register(CONFIG, 1) == 0x08) ? printf("SUCCESS!! Set as PRX\n") : printf("FAILED!!\n");
    *(nrf_tbuffer+1) = PAYLOAD_BYTES; 
    w_register(RX_PW_P0, nrf_tbuffer, 1);
    #endif
    
    reset_address(); 
    set_address();

    #if RECEIVER_OR_TRANSMITTER
    //Clear TX Buffers
    flush_tx();
    #endif

    // Ensures that status register flags are clear! When flags are still enabled the nrf24l01+ cannot 
    //  transmit or receive.
    status_reg = *r_register(STATUS, 1); 
    status_reg |= 0x70;
    w_register(STATUS, &status_reg, 1);

    // Auto ack is enabled by default so no need to write to ENAA if you want auto ack
    if (NO_ACK){
        *(nrf_tbuffer+1) = (*r_register(EN_AA, 1) & 0x00); 
        w_register(EN_AA, nrf_tbuffer, 1);
        #if RECEIVER_OR_TRANSMITTER
        *(nrf_tbuffer+1) = (*r_register(SETUP_RETR, 1) & 0x00);
        w_register(SETUP_RETR, nrf_tbuffer, 1);        
        #endif
    }
}

static void reset_address(void){
    for (int i = 0; i < RESET_ADDRESS_SIZE; i++){
        *(nrf_tbuffer+i+1) = (RESET_ADDRESS >> 8*i) & 0xFF;  
    }   

    w_register(TX_ADDR, nrf_tbuffer, RESET_ADDRESS_SIZE);
    r_register(TX_ADDR, RESET_ADDRESS_SIZE);
 
    for (int i = 0; i < RESET_ADDRESS_SIZE; i++){
        if ( *(nrf_rbuffer + i) != (((RESET_ADDRESS) >> 8*i) & 0xFF) ){
            printf("FAILED -- TX_ADDR reset\n");
        } 
    }

    w_register(RX_ADDR_P0, nrf_tbuffer, RESET_ADDRESS_SIZE);
    r_register(RX_ADDR_P0, RESET_ADDRESS_SIZE);
 
    for (int i = 0; i < RESET_ADDRESS_SIZE; i++){
        if ( *(nrf_rbuffer + i) != (((RESET_ADDRESS) >> 8*i) & 0xFF) ){
            printf("FAILED -- RX_ADDR_P0 reset\n");
        } 
    }
    
}

static void set_address(void){
    *(nrf_tbuffer+1) = ADDRESS_SIZE - 2;
    w_register(SETUP_AW, nrf_tbuffer, 1);
    ((*r_register(SETUP_AW, 1)) == (ADDRESS_SIZE - 2)) ? printf("SUCESS! Address size set\n") : printf("FAILED\n");
    
    for (int i = 0; i < ADDRESS_SIZE; i++){
        *(nrf_tbuffer+i+1) = (ADDRESS >> 8*i) & 0xFF;  
    }   

    #if RECEIVER_OR_TRANSMITTER
    w_register(TX_ADDR, nrf_tbuffer, ADDRESS_SIZE);
    r_register(TX_ADDR, ADDRESS_SIZE);
     
    for (int i = 0; i < ADDRESS_SIZE; i++){
        if ( *(nrf_rbuffer + i) != (((ADDRESS) >> 8*i) & 0xFF) ){
            printf("FAILED\n");
        } 
    }
 
    printf("Successfull TX_ADDR read and write\n");

    w_register(RX_ADDR_P0, nrf_tbuffer, ADDRESS_SIZE);
    r_register(RX_ADDR_P0, ADDRESS_SIZE);
     
    for (int i = 0; i < ADDRESS_SIZE; i++){
        if ( *(nrf_rbuffer + i) != (((ADDRESS) >> 8*i) & 0xFF) ){
            printf("FAILED\n");
        } 
    }
 
    printf("Successfull RX_ADDR_P0 read and write\n");
    #else
    w_register(RX_ADDR_P0, nrf_tbuffer, ADDRESS_SIZE);
    r_register(RX_ADDR_P0, ADDRESS_SIZE);
     
    for (int i = 0; i < ADDRESS_SIZE; i++){
        if ( *(nrf_rbuffer + i) != (((ADDRESS) >> 8*i) & 0xFF) ){
            printf("FAILED\n");
        } 
    }
 
    printf("Successfull RX_ADDR_0 read and write\n");
    #endif    
}

static unsigned char* r_register(unsigned char address, unsigned char return_size){
    /*
        To read from a register you transmit the 5 bit register map address of the 
        register you wish to read from. All register addresses 
        are a byte so we cast the int enum values to 8 bit/1 byte values. Then a 
        a 5 bit mask is applied to satisfy the command requirements.

        transmit buffer size must always be greater than receive buffer size

        below is the line that performs the necessary adjustments to the reg addresses
    */
    *nrf_tbuffer = address & 0x1F; // 0x1F applies the 5 bit mask 

    return spi_transmit(nrf_tbuffer, return_size+1, nrf_rbuffer, return_size);
}

// Byte addresse bits are label 0 to 7 -- 0 being the LSb and 7 being the MSb
// So to change the 2nd bit (bit 0) bit_to_toggle would be 0
//    to change the 3rd bit (bit 2) bit_to_toggle would be 2
// turn_on_off --- 1 to turn on : 0 to turn off
static void w_register(unsigned char address, unsigned char* data, unsigned char size){
    /*
        To write to a register you transmit the 5 bit register map address of the register
        with the 6th bit turned on.You then follow the command with 
        the desired data to be written. 

        below are the lines that prepare the data and command for transmission
    */
    *(nrf_tbuffer) = (address & 0x1F) | 0x20; // 0x1F applies the 5 bit mask and (... | 0x20) turns on the 6th bit 
   
    if (data != nrf_tbuffer){
        int i = 0;
        while (++i <= size){
            *(nrf_tbuffer + i) = *(data + (i-1));
        } 
    }

    // size is bytes of data so you add 1 for the cmd byte
    spi_transmit(nrf_tbuffer, size+1, nrf_rbuffer, 0);
}   // %04X --- formatting (0 - padding done with 0s, 2 - atleast 2 characters wide, X - hexadecimal)
    // printf("0x%02X ", *reg_data);     

#if RECEIVER_OR_TRANSMITTER 
//include function declarations and definitions for transmitter in this section
static void set_as_ptx(void){
    config_reg = (*r_register(CONFIG, 1)) & ~(0x01);
    w_register(CONFIG, &config_reg, 1);
    (*r_register(CONFIG, 1) == config_reg) ? printf("SUCCESSFULLY set as PTX\n") : printf("FAILED!!\n");
}

static void high_pulse_ptx(void){
    set_ce_high();
    usleep(20);
    set_ce_low();
    
    //printf("SUCCESSFUL HIGH PULSE FROM PTX\n");
}
// Transaction starts from byte 0 -- meaning LSB is transmitted first followed by 1 then 2 ... 
//  The bytes however are transmitted MSB to LSB -- meaning the 7th bit is transmitted followed by 6 then 5 ...
// 
// 
// Max size of data is 32 bytes since that is the max size payload nrf24l01 can transmit 
static void w_tx_payload(unsigned char* data, unsigned char size){
    #if !NO_ACK
    flush_tx();
    #endif
    
    unsigned char i = 0;
    
    *nrf_tbuffer = 0xA0; // payload write command for nrf24l01 (1010 0000)
    
    while (i < size){
        *(nrf_tbuffer + (i+1)) = *(data + i);
        i++;
    }    

    spi_transmit(nrf_tbuffer, size+1, nrf_rbuffer, 0); // size+1 is data+cmd
}

static void transmit_payload(void){
    // Activates PWR_UP mode by setting to 1
    config_reg |= 0x02;
    w_register(CONFIG, &config_reg, 1);
    high_pulse_ptx();

    #if !NO_ACK
    // TX_DS bit in status register is set high if ack was received
    // MAX_RT bit in status register is set high when max retransmissions have been sent
    while (!((status_reg = *r_register(STATUS, 1)) >> 4)); // in data sheet 4 MSbits are 0 when no flags are on

    /*
    if ((status_reg >> 4) == 0x01){
        //ERROR --- MAX_RT is enabled
        printf("Transmission failed!!");
        // RESET MAX_RT --- MAX_RT reset value is 0 however accoring
        //  to the datasheet instead of directly writing a 0 you must 
        //  reset by writing a 1 instead.
            }
    else if ((status_reg >> 4) == 0x02){
        printf("SUCCESSFUL TRANSMISSION!!!\n");
    }*/

    status_reg |= 0x30;
    w_register(STATUS, &status_reg, 1);
    #else 
    //status_reg |= 0x20;
    //status register already has reset value saved in it so no need to modify
    w_register(STATUS, &status_reg, 1);
    #endif

    // Deactivates PWR_UP mode by setting to 0
    config_reg &= ~0x02;
    w_register(CONFIG, &config_reg, 1);
}

static void flush_tx(void){
    *nrf_tbuffer = 0xE1;    
    spi_transmit(nrf_tbuffer, 1, NULL, 0);
}

void nrf_send(unsigned char* data){
    w_tx_payload(data, PAYLOAD_BYTES);
    transmit_payload(); 
}
#else 
//include function delcarations and definitions for receiver in this section

static void set_as_prx(void){
    config_reg = (*r_register(CONFIG, 1)) | 0x01;
    w_register(CONFIG, &config_reg, 1);
    (*r_register(CONFIG, 1) == config_reg) ? printf("SUCCESSFULLY set as PRX\n") : printf("FAILED!!\n");
}

static void high_pulse_prx(void){
    set_ce_high();
    usleep(130);
    
    //printf("SUCCESSFUL HIGH PULSE FROM PRX\n");
}

static unsigned char* r_rx_payload(void){
   *nrf_tbuffer = 0x61; // payload read command for nrf24l01 (0110 0001)

    //Maybe consider saving in nrf buffer then passing pointer to nrf buffer

   return spi_transmit(nrf_tbuffer, PAYLOAD_BYTES+1, nrf_rbuffer, PAYLOAD_BYTES); // +1 for the cmd
}

// payload size will always be of size PAYLOAD_BYTES
static void receive_payload(void){
    flush_rx();
    
    // Activates PWR_UP mode by setting to 1
    config_reg |= 0x02;
    w_register(CONFIG, &config_reg, 1);
    high_pulse_prx();
    
    while ( !((status_reg = *r_register(STATUS, 1)) >> 4));

    set_ce_low();
    // followed by a r_rx_payload call from MCU
    
    // Deactivates PWR_UP mode by setting to 0
    config_reg &= ~0x02;
    w_register(CONFIG, &config_reg, 1);

    status_reg |= 0x40;
    w_register(STATUS, &status_reg, 1); 
}

static void flush_rx(void){
    *nrf_tbuffer = 0xE2;
    spi_transmit(nrf_tbuffer, 1, NULL, 0);
}

void nrf_read(unsigned char * rdata){
    receive_payload();
    r_rx_payload(); // returns pointer to nrf_rbuffer

    for (int i = 0; i < PAYLOAD_BYTES; i++){
        *(rdata + i) = *(nrf_rbuffer + i);
    }
}
#endif


