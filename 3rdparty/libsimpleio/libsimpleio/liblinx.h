// LabView LINX device firmware definitions

// Copyright (C)2016-2023, Philip Munts dba Munts Technologies.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef _LIBLINX_H_
#define _LIBLINX_H_

#include <unistd.h>
#ifndef _BEGIN_STD_C
#include <libsimpleio/cplusplus.h>
#endif
#include <stdint.h>

#define LINX_SOF	0xFF
#define LINX_VERSION	0x03000000

// LINX command structure

typedef struct
{
  uint8_t SoF;
  uint8_t PacketSize;
  uint16_t PacketNum;
  uint16_t Command;
  uint8_t Args[54];
} LINX_command_t;

// LINX response structure

typedef struct
{
  uint8_t SoF;
  uint8_t PacketSize;
  uint16_t PacketNum;
  uint8_t Status;
  uint8_t Data[55];
} LINX_response_t;

// LINX command codes

#define CMD_SYNC			0x0000
#define CMD_FLUSH			0x0001
#define CMD_SYSTEM_RESET		0x0002
#define CMD_GET_DEVICE_ID		0x0003
#define CMD_GET_LINX_API_VERSION	0x0004
#define CMD_GET_MAX_BAUD_RATE		0x0005
#define CMD_SET_BAUD_RATE		0x0006
#define CMD_GET_MAX_PACKET_SIZE		0x0007
#define CMD_GET_GPIO_CHANNELS		0x0008
#define CMD_GET_ANALOG_IN_CHANNELS	0x0009
#define CMD_GET_ANALOG_OUT_CHANNELS	0x000A
#define CMD_GET_PWM_CHANNELS		0x000B
#define CMD_GET_QE_CHANNELS		0x000C
#define CMD_GET_UART_CHANNELS		0x000D
#define CMD_GET_I2C_CHANNELS		0x000E
#define CMD_GET_SPI_CHANNELS		0x000F
#define CMD_GET_CAN_CHANNELS		0x0010
#define CMD_DISCONNECT			0x0011
#define CMD_SET_DEVICE_USER_ID		0x0012
#define CMD_GET_DEVICE_USER_ID		0x0013
#define CMD_SET_DEVICE_ETHERNET_IPADDR	0x0014
#define CMD_GET_DEVICE_ETHERNET_IPADDR	0x0015
#define CMD_SET_DEVICE_ETHERNET_TCPPORT	0x0016
#define CMD_GET_DEVICE_ETHERNET_TCPPORT 0x0017
#define CMD_SET_DEVICE_WIFI_IPADDR	0x0018
#define CMD_GET_DEVICE_WIFI_IPADDR	0x0019
#define CMD_SET_DEVICE_WIFI_TCPPORT	0x001A
#define CMD_GET_DEVICE_WIFI_TCPPORT	0x001B
#define CMD_SET_DEVICE_WIFI_SSID	0x001C
#define CMD_GET_DEVICE_WIFI_SSID	0x001D
#define CMD_SET_DEVICE_WIFI_SECURITY	0x001E
#define CMD_GET_DEVICE_WIFI_SECURITY	0x001F
#define CMD_SET_DEVICE_WIFI_PASSWORD	0x0020
#define CMD_GET_DEVICE_WIFI_PASSWORD	0x0021
#define CMD_SET_DEVICE_LINX_MAX BAUD RATE	0x0022
#define CMD_GET_DEVICE_LINX_MAX_BAUD_RATE	0x0023
#define CMD_GET_DEVICE_NAME		0x0024
#define CMD_GET_SERVO_CHANNELS		0x0025
#define CMD_GPIO_CONFIGURE		0x0040
#define CMD_GPIO_WRITE			0x0041
#define CMD_GPIO_READ			0x0042
#define CMD_GPIO_SQUARE_WAVE		0x0043
#define CMD_GPIO_PULSE_WIDTH		0x0044
#define CMD_SET_ANALOG_REFERENCE	0x0060
#define CMD_GET_ANALOG_REFERENCE	0x0061
#define CMD_SET_ANALOG_RESOLUTION	0x0062
#define CMD_GET_ANALOG_RESOLUTION	0x0063
#define CMD_ANALOG_READ			0x0064
#define CMD_ANALOG_WRITE		0x0065
#define CMD_PWM_OPEN			0x0080
#define CMD_PWM_SET_MODE		0x0081
#define CMD_PWM_SET_FREQUENCY		0x0082
#define CMD_PWM_SET_DUTYCYCLE		0x0083
#define CMD_PWM_CLOSE			0x0084
#define CMD_UART_OPEN			0x00C0
#define CMD_UART_SET_BAUD_RATE		0x00C1
#define CMD_UART_GET_BYTES_AVAILABLE	0x00C2
#define CMD_UART_READ			0x00C3
#define CMD_UART_WRITE			0x00C4
#define CMD_UART_CLOSE			0x00C5
#define CMD_I2C_OPEN			0x00E0
#define CMD_I2C_SET_SPEED		0x00E1
#define CMD_I2C_WRITE			0x00E2
#define CMD_I2C_READ			0x00E3
#define CMD_I2C_CLOSE			0x00E4
#define CMD_SPI_OPEN			0x0100
#define CMD_SPI_SET_BIT_ORDER		0x0101
#define CMD_SPI_SET_CLOCK_FREQUENCY	0x0102
#define CMD_SPI_SET_MODE		0x0103
#define CMD_SPI_SET_FRAME_SIZE		0x0104
#define CMD_SPI_SET_CS_LOGIC_LEVEL	0x0105
#define CMD_SPI_SET_CS_PIN		0x0106
#define CMD_SPI_WRITE_READ		0x0107
#define CMD_SERVO_OPEN			0x0140
#define CMD_SERVO_SET_PULSE_WIDTH	0x0141
#define CMD_SERVO_CLOSE			0x0142
#define CMD_WS2812_OPEN			0x0160
#define CMD_WS2812_WRITE_ONE_PIXEL	0x0161
#define CMD_WS2812_WRITE_N_PIXELS	0x0162
#define CMD_WS2812_REFRESH		0x0163
#define CMD_WS2812_CLOSE		0x0164
#define CMD_CUSTOM_BASE			0xFC00
#define CMD_CUSTOM0			(CMD_CUSTOM_BASE + 0)
#define CMD_CUSTOM1			(CMD_CUSTOM_BASE + 1)
#define CMD_CUSTOM2			(CMD_CUSTOM_BASE + 2)
#define CMD_CUSTOM3			(CMD_CUSTOM_BASE + 3)
#define CMD_CUSTOM4			(CMD_CUSTOM_BASE + 4)
#define CMD_CUSTOM5			(CMD_CUSTOM_BASE + 5)
#define CMD_CUSTOM6			(CMD_CUSTOM_BASE + 6)
#define CMD_CUSTOM7			(CMD_CUSTOM_BASE + 7)
#define CMD_CUSTOM8			(CMD_CUSTOM_BASE + 8)
#define CMD_CUSTOM9			(CMD_CUSTOM_BASE + 9)

// LINX status codes

#define L_OK				0x00
#define L_FUNCTION_NOT_SUPPORTED	0x01
#define L_REQUEST_RESEND		0x02
#define L_UNKNOWN_ERROR			0x03
#define L_DISCONNECT			0x04

// LINX packet framing routines

_BEGIN_STD_C

extern void LINX_transmit_command(int32_t fd, LINX_command_t *cmd,
  int32_t *error);

extern void LINX_receive_command(int32_t fd, LINX_command_t *cmd,
  int32_t *count, int32_t *error);

extern void LINX_transmit_response(int32_t fd, LINX_response_t *resp,
  int32_t *error);

extern void LINX_receive_response(int32_t fd, LINX_response_t *resp,
  int32_t *count, int32_t *error);

// Byte packing and unpacking routines

// For all of the following routines, byte 0 is the most
// significant byte, and byte 1 or byte 3 is the least
// significant byte (i.e. network byte order)

extern uint16_t LINX_makeu16(uint8_t b0, uint8_t b1);

extern uint32_t LINX_makeu32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);

extern uint8_t LINX_splitu16(uint16_t u16, int32_t bn);

extern uint8_t LINX_splitu32(uint32_t u32, int32_t bn);

_END_STD_C

#endif
