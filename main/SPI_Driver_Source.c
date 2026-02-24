#include "SPI_Driver_Header.h"

static unsigned char cmd_buffer; // buffer that holds cmd
static unsigned char trx_buffer[TX_BUFFER_SIZE]; // buffer that holds data
static unsigned char trx_size; // bytes being transmitted -- max 32
static unsigned char rcx_size; // bytes being received -- max 32
static unsigned char rcx_buffer[RX_BUFFER_SIZE]; // buffer that holds received data 

static spi_device_handle_t handle;
static void transaction(void);

void init_spi_driver(void){
    
    // Specify pins on bus
    spi_bus_config_t spi_config = {
        .mosi_io_num = MOSI,
        .miso_io_num = MISO,
        .sclk_io_num = SCLK
    };

    spi_device_interface_config_t spi_device_config = {
        .command_bits = COMMAND_BITS,
        .clock_speed_hz = CLOCK_SPEED,
        .mode = MODE,
        .spics_io_num = CS,
        .queue_size = QUEUE_SIZE
    };
    
    spi_bus_initialize(SPI2_HOST, &spi_config, 1);
    spi_bus_add_device(SPI2_HOST, &spi_device_config, &handle); 
}

// If you expect data, it will be stored in the rx_buffer after transmission 
//
// Receiving data will require a cmd so a transmit is required therefore only
//  one functions is needed.
unsigned char* spi_transmit(unsigned char* data, unsigned char t_size, unsigned char r_size){
    if (t_size > 0){
        trx_size = t_size - 1;
        rcx_size = r_size;
        
        // unsigned char is bounded by 0 so once size == 0 you exit the loop
        // After exiting loop you assign the 0th element its value 
        while (--t_size > 0){
            *(trx_buffer + t_size) = *(data + t_size);
        }
        
        cmd_buffer = *(data + t_size);
    
        transaction(); 

        return rcx_buffer;
    }
    else {
        // Error
    }

    return NULL;
}

static void transaction(void){
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    
    t.cmd = cmd_buffer;
    t.length = trx_size*8;
    t.rxlength = rcx_size*8;
    t.tx_buffer = trx_buffer;
    t.rx_buffer = rcx_buffer;
    
    spi_device_transmit(handle, &t);
}



