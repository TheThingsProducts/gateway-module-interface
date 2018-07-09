// Copyright © 2018 The Things Network Foundation, The Things Products B.V.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_GATEWAY_MODULE_INTERFACE_H_
#define LIB_GATEWAY_MODULE_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    GATEWAY_MODULE_CMD_NONE            = 0,
    GATEWAY_MODULE_CMD_FACTORY         = 0x6E, // Host -> Module Restores the factory default settings.
    GATEWAY_MODULE_CMD_SAVE            = 0x21, // Host -> Module Saves the current settings to EEPROM.
    GATEWAY_MODULE_CMD_SETUART         = 0x42, // Host -> Module Sets the UART baud rate to the Host.
    GATEWAY_MODULE_CMD_GETUART         = 0x62, // Host -> Module Returns the Host UART baud rate setting.
    GATEWAY_MODULE_CMD_START           = 0x30, // Host -> Module Startup RF Module.
    GATEWAY_MODULE_CMD_STOP            = 0x31, // Host -> Module Stop RF Module.
    GATEWAY_MODULE_CMD_SEND            = 0x32, // Host -> Module Host Send LoRa Packet.
    GATEWAY_MODULE_CMD_RECEIVE         = 0x33, // Module -> Host Host Receive LoRa Packet.
    GATEWAY_MODULE_CMD_RFCONFIG        = 0x34, // Host -> Module Configure RF(0-1) chain center frequency.
    GATEWAY_MODULE_CMD_IFCONFIG        = 0x35, // Host -> Module Configure IF chain(0-7) center frequency.
    GATEWAY_MODULE_CMD_IF8CONFIG       = 0x36, // Host -> Module Configure IF8 chain Bandwidth and Data Rate.
    GATEWAY_MODULE_CMD_IF9CONFIG       = 0x37, // Host -> Module Configure IF9 chain Bandwidth and Data Rate.
    GATEWAY_MODULE_CMD_TXABORT         = 0x38, // Host -> Module Aborts any current or scheduled transmission.
    GATEWAY_MODULE_CMD_TXSTATUS        = 0x39, // Host -> Module Returns radio transmitter status.
    GATEWAY_MODULE_CMD_VERSION         = 0x3A, // Host -> Module Returns Module version information.
    GATEWAY_MODULE_CMD_RFCHAIN         = 0x3B, // Host -> Module Returns RF(0-1) chain settings.
    GATEWAY_MODULE_CMD_IFCHAIN         = 0x3C, // Host -> Module Returns IF chain(0-7) settings.
    GATEWAY_MODULE_CMD_IF8CHAIN        = 0x3D, // Host -> Module Returns IF8 chain settings.
    GATEWAY_MODULE_CMD_IF9CHAIN        = 0x3E, // Host -> Module Returns IF9 chain settings.
    GATEWAY_MODULE_CMD_SETLEDS         = 0x3F, // Host -> Module Sets the output LED’s state.
    GATEWAY_MODULE_CMD_SETSYNC         = 0x40, // Host -> Module Sets the LoRa IF channels Sync Word.
    GATEWAY_MODULE_CMD_GETSYNC         = 0x41, // Host -> Module Returns the LoRa IF channels Sync Word.
    GATEWAY_MODULE_CMD_RXSTATUS        = 0x43, // Host -> Module Returns the LoRa Receive status.
    GATEWAY_MODULE_CMD_BOOTLOADER_MODE = 0x50, // Host -> Module Invalidates the checksum of the application. Need to
                                               // followed by a RESET command to switch to the Bootloader.
    GATEWAY_MODULE_CMD_RESET   = 0x51,         // Host -> Module Resets the module
    GATEWAY_MODULE_CMD_SENDCW  = 0x25,         // Host -> Module Enable CW Mode (Continuous Transmit)
    GATEWAY_MODULE_CMD_INVALID = 0xFF,         // Module -> Host Previous command received from Host does not exist
    GATEWAY_MODULE_CMD_MFGDATA = 0x07          // Host -> Module Program Manufact
} GATEWAY_MODULE_CMDS_t;

typedef void (*gateway_module_interface_write_lock_t)(bool lock);
typedef bool (*gateway_module_interface_write_t)(uint8_t* data, size_t size);
typedef bool (*gateway_module_signal_wait_t)(int timeout);
typedef void (*gateway_module_signal_set_t)(void);
typedef void (*gateway_module_receive_callback_t)(uint8_t* data, size_t size);
typedef void (*gateway_module_log_t)(const char* format, ...);

void GatewayModuleInterface_init(gateway_module_interface_write_lock_t write_lock,
                                 gateway_module_interface_write_t write, gateway_module_signal_wait_t signal_wait,
                                 gateway_module_signal_set_t       signal_set,
                                 gateway_module_receive_callback_t receive_callback, gateway_module_log_t log);
bool GatewayModuleInterface_sendCommandWaitAnswer(GATEWAY_MODULE_CMDS_t cmd, uint8_t* cmd_payload,
                                                  size_t cmd_payload_size, uint8_t* ans_payload,
                                                  size_t ans_payload_max_size);
bool GatewayModuleInterface_sendCommandWaitAck(GATEWAY_MODULE_CMDS_t cmd, uint8_t* cmd_payload,
                                               size_t cmd_payload_size);
void GatewayModuleInterface_sendAck(GATEWAY_MODULE_CMDS_t cmd, bool ack);
void GatewayModuleInterface_dispatch(uint8_t d);

#endif /* LIB_GATEWAY_MODULE_INTERFACE_H_ */
