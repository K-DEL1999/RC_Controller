#include "nRF24L01_PLUS_Header.h"
#include "SPI_Driver_Header.h"
#include <stdio.h>

#include "driver/gptimer.h"

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

#if RECEIVER_OR_TRANSMITTER
// Transmitter
static void set_as_ptx(void);
static void w_tx_payload(unsigned char* data, unsigned char size);
#else
// Receiver
static void set_as_prx(void);
static unsigned char* r_rx_payload(void);
#endif

// ======================== //
// Timer set up
// ======================== //
#define PULSE_TIME_US ((RECEIVER_OR_TRANSMITTER) ? 20 : 150)

// do we really need this flag ?
static volatile bool high_pulse_complete; // signals when high pulse is complete with a 1
// 

static gptimer_handle_t gptimer; 
static bool timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
static void init_gptimer(void);
// ======================== //
// ======================== //

void configure_transceiver(void){
    init_gptimer();
    init_spi_driver();
    
    // set as primary transmitter or primary receiver
    #if RECEIVER_OR_TRANSMITTER
    set_as_ptx();
    //(*r_register(CONFIG, 1) == 0x09) ? printf("SUCCESS!! Set as PTX\n") : printf("FAILED!!\n");
    #else
    set_as_prx();
    //(*r_register(CONFIG, 1) == 0x08) ? printf("SUCCESS!! Set as PRX\n") : printf("FAILED!!\n");
    #endif
    
    reset_address(); 
    set_address();

    // Auto ack is enabled by default so no need to write to ENAA

    set_ce_high();
    gptimer_start(gptimer);
    while (!high_pulse_complete);    
    high_pulse_complete = 0;
    
    printf("SUCCESSFUL HIGH PULSE 1!!!\n");

    set_ce_high();
    gptimer_start(gptimer);
    while (!high_pulse_complete);    
    high_pulse_complete = 0;

    printf("SUCCESSFUL HIGH PULSE 2!!!\n");
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
// ========================================================= //
// NOTT TESTED !!!!
// ========================================================= //
// Transaction starts from byte 0 -- meaning LSB is transmitted first followed by 1 then 2 ... 
//  The bytes however are transmitted MSB to LSB -- meaning the 7th bit is transmitted followed by 6 then 5 ...
// 
// 
// Max size of data is 32 bytes since that is the max size payload nrf24l01 can transmit 

static void set_as_ptx(void){
    unsigned char data = (*r_register(CONFIG, 1)) & ~(0x01);
    w_register(CONFIG, &data, 1);
    (*r_register(CONFIG, 1) == data) ? printf("SUCCESSFULLY set as PTX\n") : printf("FAILED!!\n");
}


static void w_tx_payload(unsigned char* data, unsigned char size){
    unsigned char d_size = size;
    
    *nrf_tbuffer = 0xA0; // payload write command for nrf24l01 (1010 0000)
    
    while (d_size){
        *(nrf_tbuffer + d_size--) = *data; // changes the data from MSB-LSB to LSB-MSB
    }    

    spi_transmit(nrf_tbuffer, size+1, nrf_rbuffer, 0); // size+1 is data+cmd
}

// ========================================================= //
// NOTT TESTED !!!!
// ========================================================= //

static void transmit_payload(void){
}
#else 
//include function delcarations and definitions for receiver in this section

// ========================================================= //
// NOTT TESTED !!!!
// ========================================================= //

static void set_as_prx(void){
    unsigned char data = (*r_register(CONFIG, 1)) | 0x01;
    w_register(CONFIG, &data, 1);
    (*r_register(CONFIG, 1) == data) ? printf("SUCCESSFULLY set as PRX\n") : printf("FAILED!!\n");
}

static unsigned char* r_rx_payload(void){
   *nrf_buffer = 0x61; // payload read command for nrf24l01 (0110 0001)

    //Maybe consider saving in nrf buffer then passing pointer to nrf buffer

   return spi_transmit(nrf_buffer, 1, 32);
}
// ========================================================= //
// NOTT TESTED !!!!
// ========================================================= //
#endif

// to do: change to esp_timer --- gptimer is giving inconsistent results with one shot timer
// =============================================== //
// GPTimer initialization
// =============================================== //
static bool timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx){
    set_ce_low();
    high_pulse_complete = 1;
    gptimer_stop(timer);

    return false;
}

static void init_gptimer(void){
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000
    }; 

    gptimer_new_timer(&timer_config, &gptimer);

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = PULSE_TIME_US,
        .flags.auto_reload_on_alarm = false
    };

    gptimer_set_alarm_action(gptimer, &alarm_config);
    
    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_callback
    };

    gptimer_register_event_callbacks(gptimer, &cbs, NULL);
    gptimer_enable(gptimer);
}
// =============================================== //
// =============================================== //


