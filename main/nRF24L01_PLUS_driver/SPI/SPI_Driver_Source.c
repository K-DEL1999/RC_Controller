#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "SPI_Driver_Header.h"

static spi_device_handle_t handle;

#if NRF24L01_PLUS_CE
static void init_ce_pin(void){
    gpio_config_t io_conf = {};

    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    
    gpio_config(&io_conf);
}

void set_ce_high(void){
    gpio_set_level(CE, 1);
}

void set_ce_low(void){
    gpio_set_level(CE, 0);
}
#endif

void init_spi_driver(void){
    
    // Specify pins on bus
    spi_bus_config_t spi_config = {
        .mosi_io_num = MOSI,
        .miso_io_num = MISO,
        .sclk_io_num = SCLK
    };

    // mode refers to clock polarty and clock phase. MODE is set to 0 in the header file
    //  which translates to (0,0) -> (CPOL,CPHAS) -> meaning clock polarity is 0 and clock phase is 0 
    spi_device_interface_config_t spi_device_config = {
        .command_bits = COMMAND_BITS,
        .clock_speed_hz = CLOCK_SPEED,
        .mode = MODE,
        .spics_io_num = CS,
        .queue_size = QUEUE_SIZE
    };
    
    spi_bus_initialize(SPI2_HOST, &spi_config, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &spi_device_config, &handle);

    #if NRF24L01_PLUS_CE
    init_ce_pin();
    #endif 
}

// If you expect data, it will be stored in the rx_buffer after transmission 
//
// Receiving data will require a cmd so a transmit is required therefore only
//  one functions is needed.
unsigned char* spi_transmit(unsigned char* tdata, unsigned char tsize, unsigned char* rdata, unsigned char rsize){
    if (tsize == 1){
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        
        t.cmd = *tdata;
        t.length = 0; // Subtract 1 because tdata contains cmd and data. We just want size of data
        t.rxlength = 0;
        t.tx_buffer = NULL;
        t.rx_buffer = NULL;
        
        spi_device_transmit(handle, &t);
    }
    else if (tsize > 1){
        // data includes the command for the cmd buffer and the data for the tx_buffer. Since the
        //  cmd is passed to the cmd member of the SPI transaction it is not included in the t.tx_buffer
        //  so you have to subtract 1 from the t_size to get the number of bytes that will be passed
        //  in the t.tx_buffer. (tsize is the cmd + data) 
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        
        t.cmd = *tdata;
        t.length = (tsize - 1)*8; // Subtract 1 because tdata contains cmd and data. We just want size of data
        t.rxlength = rsize*8;
        t.tx_buffer = (tdata + 1);
        t.rx_buffer = rdata;
        
        spi_device_transmit(handle, &t);

        return rdata;
    }
    else {
        // Error
    }

    return NULL;
}



