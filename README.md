# RC_Controller

## Datasheet conventions

Commands, bit state conditions, and register names are written in _courier_
Pin names and pin signal conditions are written in **courier_bold**


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

**Shockburst will handle all the packet assembly and packet validation**

<img width="1347" height="833" alt="image" src="https://github.com/user-attachments/assets/ddffd89d-fe26-4ae4-8f6f-5be1d6e38808" />
   
Flow chart for how enhanced shockburst handles packet transmission and reception





