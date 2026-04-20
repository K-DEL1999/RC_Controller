# RC_Controller Implementation in ESP32 

This project is meant as an add on to my drone project. It combines an ESP32 WROOM with nRF24L01+ module and 2 joysticks for the purpose of controlling the drones movement. The built in SPI peripheral is used to send commands to the nRF24L01+. The ESP32s ADC is also used to receive input from the joysticks. The input from the joysticks is sent as a payload to the nRF24L01+ which is then transmitted to the flight controller on the drone and used to determine the drones movement. Included in this project are an SPI Driver, an nRF24L01+ Driver and an a few functions that utilize the ADC peripheral on the ESP32 WROOM. 
  
Datasheet conventions: Commands, bit state conditions, and register names are written in _courier_
Pin names and pin signal conditions are written in **courier_bold**

## Project Structure

- **RC_Controller_Source.c**: function defintions, helper function declarations, helper function declarations
- **RC_Controller_Header.h**: function declarations and struct defintions
- **RC_Controller_Main.c**: example showing how to use high level functions
- **nRF24L01_PLUS_Source.c**: All low level function declarations and definitions
- **nRF24L01_PLUS_Header.h**: function declarations and struct definitions
- **SPI_Driver_Header.h**: SPI transmit declaration
- **SPI_Driver_Source.c**: SPI transmit definition and helper function declarations and definitions

## Provided High Level Functions

Users are provided a send and receive function but can choose to design their own using the lower level functions.

```c
void configure_transceiver(void); 
void nrf_send(unsigned char* data);
void nrf_read(unsigned char* rdata);
```

## Low Level Fucntions

```c
// All size references are in bytes -- how many unsigned char elements there are 
static unsigned char* r_register(unsigned char address, unsigned char return_size); 
static void w_register(unsigned char address, unsigned char* data, unsigned char size);
static void set_address(void);
static void reset_address(void);

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
```

## Connection

This device is configured and operated through SPI. Through SPI the register map is available. What is a register map ? Special memory area that consists of named addresses called registers (control and status registers (CSR)). Register map contains all configuration registers in the nRF24L01.

| PIN | Function |
| :---: | :---: |
| CE | Chip enable activates RX or TX mode
| CSN | SPI chip select |
| SCK | SPI clock |
| MOSI | SPI slave data input | 
| MISO | SPI slave data output | 
| IRQ | OPTIONAL (maskable interrupt pin) |
| VCC | Power supply (+1.9V to +3.6V DC) |
| VSS | GND |

## Built-in state machine 

nRF24L01 has a built-in state machine that controls the transitions between differnt operating modes of the chip. The state machine takes input from user defined register values and internal singals.

(**state machine** - an abstract computational model that can be in exactly one of a finite number of states at any
given time)

The nRF24L01 is in an undefined state until VDD becomes greater than or equal to 1.9V. Once in a defined state it enters the **Power On Reset State**. It remains in this state until it enters the **Power Down Mode**. Even when inpower down mode the MCU can control the chip through *SPI* and the *Chip Enable pin*.

<img width="1547" height="1149" alt="nrf24L01_plus_state_diagram" src="https://github.com/user-attachments/assets/9521496b-50d0-4d86-b810-fdfbb2d13923" />

## States

- **Undefined** : before VDD reaches 1.9V
- **Recommended operating mode** : Used during normal operation.
- **Possible operating mode** : State that is allowed to use but it is not used during normal operation
- **Transition state** : Time limited state used during startup of the oscillator and settling in the PLL (Phase-Locked Loop). PLL is crucial part of the integrated frequency synthesizer used to generate the stable, high frequency carrier singal (2.4 Ghz) needed for GFSK modulation. Locks the output frequency of the radio to a specific 2.4 Ghz channel.

## Modes

### Power Down Mode (pg 20)

nRF24L01 is disbaled with minial current consumption. All register values available from the SPI are maintained and the SPI can be activated.

Power Down mode is entered by setting the PWR_UP bit in the CONFIG register low (0)

```c
// Deactivates PWR_UP mode by setting to 0
config_reg &= ~0x02;
w_register(CONFIG, &config_reg, 1);
```

### Standby Modes** (pg 20)

Standby-I mode is entered by setting the PWR_UP bit in the CONFIG register high (1)

```c
// Activates PWR_UP mode by setting to 1
config_reg |= 0x02;
w_register(CONFIG, &config_reg, 1);
```

Once PWR_UP bit in CONFIG register is set to LOW the device enters **Standby-I mode**. This mode is used to minimize average current while maintaing short startup times. In this mode part of the crystal oscillator is active. This is the mode nRF24L01 returns to from TX or RX when CE is set low. In **Standby-II mode** much more current is used and extra clock buffers are active comapred to **standby-I**. **Standby-II** occurs when CE is held high on a PTX device with empty TX FIFO. If a new packet is uploaded to the TX FIFO, PLL starts and packet is transmitted. 

Register values are maintained during standby modes and SPI may be activated.

### RX mode (pg 21)

Active mode where nRF24L01 is a receiver. Rx mode is entered by having PWR_UP bit set high (1), PRIM_RX bit set high (1), and CE pin set high (1) packets saved in RX FIFO. If full, packet is discarded. 

_REMAINS IN RX MODE UNTIL MCU CONFIGURES IT TO STANDBY-I MODE OR POWER DOWN MODE_

```c
static void set_as_prx(void){
    config_reg = (*r_register(CONFIG, 1)) | 0x01;
    w_register(CONFIG, &config_reg, 1);
    (*r_register(CONFIG, 1) == config_reg) ? printf("SUCCESSFULLY set as PRX\n") : printf("FAILED!!\n");
}
```

### TX mode 

Active mode where nRF24L01 transmits packets. Tx mode in entered by having the PWR_UP bit set high (1), PRIM_RX bit set low (0), a payload in the TX FIFO and a high
pulse on CE for more than 10 us.

nRF24L01 stays in Tx mode untile it finishes transmitting a current packet. If CE = 0, nRF24L01 returns to standby-I mode.
If CE = 1, the next action is determined by the status of the TX FIFO. If TX FIFO not empty then it remains in Tx mode, 
transmitting the next packet. If TX FIFO is empty nRF24L01 goes into stanby-II mode.

**NEVER KEEP THE nRF24LO1 IN TX MODE FOR MORE THAN 4 ms AT A TIME**
**IF AUTO RETRANSMIT IS ENABLED, THE NRF24L01 IS NEVER IN TX MODE LONG ENOUGH TO DISOBEY THE RULE**

```c
static void set_as_ptx(void){
    config_reg = (*r_register(CONFIG, 1)) & ~(0x01);
    w_register(CONFIG, &config_reg, 1);
    (*r_register(CONFIG, 1) == config_reg) ? printf("SUCCESSFULLY set as PTX\n") : printf("FAILED!!\n");
}
```

## Timing for mode transitioning

1. Power Down -> standby mode             : 1.5 ms (max)
2. Standby modes -> TX/RX                 : 130 us (max)
3. Minimun CE high for TX                 : 10 us (min)
4. Delay from CE positive edge to csn low : 4 us (min)
5. Power Down -> TX/RX                    : 1.5 ms (min) (controlled by MCU)

## Enhanced Shock Burst

This is an onboard subsystem that handles all the packet handling and timing. It deals with the packet transaction between the PTX (Primary Transmitter) and the PRX (Secondary Transmitter). Packets are formatted in the following way...

<img width="1347" height="249" alt="image" src="https://github.com/user-attachments/assets/03cbf088-d47d-4929-8139-498a6483bcc9" />
  
- **Preamble**: The preamble is a bit sequence used to detect 0 and 1 levels in the receiver
- **Address**: This is the address for the receiver. An address ensures that the correct packet are detected by the receiver. The address field can be configured to be 3, 4 or, 5 bytes long with the AW register.
- **Packet Control Field**:
      - Payload Length: This 6 bit field specifies the length of the payload in bytes. The length of the payload can be from 0 to 32 bytes.
      - PID (packet identification): The 2 bit PID field is used to detect if the received packet is new or retransmitted.
      - 1 bit no ack flag: The Selective Auto Acknowledgement feature controls the NO_ACK flag.
- **Payload**: The payload is the user defined content of the packet. It can be 0 to 32 bytes wide and is transmitted on-air as it is uploaded (unmodified) to the device
- **CRC** (Cyclic Redundancy Check) : The CRC is the error detection mechanism in the packet. It may either be 1 or 2 bytes and is calculated over the address, Packet Control Field, and Payload

**Shockburst will handle all the packet assembly and packet validation (packet acknowledgement and packet retransmission)**

<img width="1347" height="833" alt="image" src="https://github.com/user-attachments/assets/ddffd89d-fe26-4ae4-8f6f-5be1d6e38808" />
   
Flow chart for how enhanced shockburst handles packet transmission and reception.

## Register Values
 I have stored all the registers in an enumeration. The nature of an enum made it very simple to initialize all the values without having to manually input each register address.

```c
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
```


## SPI Commands

<img width="1260" height="738" alt="image" src="https://github.com/user-attachments/assets/b4686b89-46ca-4bc9-bec9-24a3d6ff3e05" />

### R_Register Function

To read from a register you transmit the 5 bit register map address of the register you wish to read from. All register addresses are a byte so we cast the int enum values to 8 bit/1 byte values. Then a a 5 bit mask is applied to satisfy the command requirements. You must also specify the size of the data being read (`return_size`)

```c
static unsigned char* r_register(unsigned char address, unsigned char return_size){
    *nrf_tbuffer = address & 0x1F; // 0x1F applies the 5 bit mask 

    return spi_transmit(nrf_tbuffer, return_size+1, nrf_rbuffer, return_size);
}
```

### R_Register Function Verification

<img width="1096" height="861" alt="nRF24L01_plus_read_cmd_CONFIG" src="https://github.com/user-attachments/assets/08d4c3ad-6ca4-4247-8cd7-11be003d3f9a" />

### W_Register Function

To write to a register you transmit the 5 bit register map address of the register with the 6th bit turned on.You then follow the command with the desired data to be written. You must include the size of the data aswell when calling the function. 

```c
static void w_register(unsigned char address, unsigned char* data, unsigned char size){
    *(nrf_tbuffer) = (address & 0x1F) | 0x20; // 0x1F applies the 5 bit mask and (... | 0x20) turns on the 6th bit 
   
    if (data != nrf_tbuffer){
        int i = 0;
        while (++i <= size){
            *(nrf_tbuffer + i) = *(data + (i-1));
        } 
    }

    // size is bytes of data so you add 1 for the cmd byte
    spi_transmit(nrf_tbuffer, size+1, nrf_rbuffer, 0);
}
```

### W_Register Function Verifiction

<img width="2172" height="906" alt="nRF24L01_PLUS_write_cmd_CONFIG" src="https://github.com/user-attachments/assets/fc650800-fc78-4dbb-aa04-5fa16f485dd6" />

### Full Initialization of PTX and RTX

To initialize the module as PTX you must set as PTX, set the address and then clear the status register to ensure no flags are set. If any flags are set the module will not transmit. To initialize the module as PRX you must set as PRX, write the payload size to the RX_PW_P0 register, set the address and then clear all flags. PTX and PRX must have the same address inorder to transmit and receive.

```c
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

    // Ensures that status register flags are clear! When flags are still enabled the nrf24l01+ cannot 
    //  transmit or receive.
    status_reg = *r_register(STATUS, 1); 
    status_reg |= 0x70;
    w_register(STATUS, &status_reg, 1);


    // Auto ack is enabled by default so no need to write to ENAA
}
```

### Initialization Verification

**Only the PTX verificaiton is included since the timing diagram will be very similar. The only change would be the extra write to the RX_PW_P0 register. Also the transmitter writes to 2 registers when setting the address - TX_ADDR and RX_ADDR_P0 - while the receiver onyl write to one address - RX_ADDR_P0**

<img width="2594" height="639" alt="nRF24L01+_PTX_Initialization" src="https://github.com/user-attachments/assets/de94a686-a909-419b-984f-e4a989c9b341" />

## Pulseview Settings

<img width="384" height="447" alt="logic_analyzer_parameters_for_nRF24L01+" src="https://github.com/user-attachments/assets/d82088fe-497b-4bfe-b936-bcf4f0ce5dbd" />

## How to Use

1. Ensure you have a working ESP32 development environment (this project was designed using the ESP-IDF development framework)
2. Clone the repository
3. Connect pins to SPI1 pins on ESP32 WROOM and connect CE pin to pin 27
4. Build and Flash Transmitter - **ensure to change RECEIVER_OR_TRANSMITTER macro to 1**
5. Build and Flash Receiver - **ensure to change RECEIVER_OR_TRANSMITTER macro to 0**

### If using ESP-IDF
```sh
idf.py build
idf.py flash
```

## Application

- **Allows wireless communication between 2 devices**














