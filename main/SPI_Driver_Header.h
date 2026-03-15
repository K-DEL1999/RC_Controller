#ifndef SPI_DRIVER_HEADER_H
#define SPI_DRIVER_HEADER_H

// If using NRF24L01_PLUS a CE pin needs to be set -- set the NRF24L01_PLUS_CE flag to 1
// enable  : 1
// disable : 0
#define NRF24L01_PLUS_CE 1

// SPI pins
#define MOSI 13
#define MISO 12
#define SCLK 14
#define CS 15

#if NRF24L01_PLUS_CE
// NRF24L01_PLUS pin to enable transmission
#define CE 27
#define GPIO_OUTPUT_PIN_SEL (1ULL << CE)
#endif

// SPI interface for slave device
#define COMMAND_BITS 8
#define CLOCK_SPEED SPI_CLK_SRC_DEFAULT
#define MODE 0 
#define QUEUE_SIZE 1

void init_spi_driver(void);
// Requires buffer for transmitted data, buffer for received data, size of data and size of expected data to be received 
unsigned char* spi_transmit(unsigned char* tdata, unsigned char tsize, unsigned char* rdata, unsigned char rsize);

#if NRF24L01_PLUS_CE
void set_ce_high(void);
void set_ce_low(void);
#endif

#endif /*SPI_DRIVER_HEADER_H*/
