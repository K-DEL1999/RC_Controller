#ifndef SPI_DRIVER_HEADER_H
#define SPI_DRIVER_HEADER_H

#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"

// SPI pins
#define MOSI 13
#define MISO 12
#define SCLK 14
#define CS 15

// SPI interface for slave device
#define COMMAND_BITS 8
#define CLOCK_SPEED SPI_CLK_SRC_DEFAULT
#define MODE 0 
#define QUEUE_SIZE 1

// SPI buffers sizes
#define TX_BUFFER_SIZE 32
#define RX_BUFFER_SIZE 32

void init_spi_driver(void);
unsigned char* spi_transmit(unsigned char* data, unsigned char t_size, unsigned char r_size);

#endif /*SPI_DRIVER_HEADER_H*/
