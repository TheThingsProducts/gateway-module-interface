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

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <iostream>
#include <thread>
#include <stdarg.h>
#include <chrono>
#include <mutex>
#include <condition_variable>

extern "C"
{
#include "gateway-module-interface.h"
}

using namespace std;

static bool init_uart(const char *uart);
static void lock_uart(bool lock);
static bool write_uart(uint8_t *data, size_t size);
static bool signal_wait(int timeout);
static void signal_set(void);
static void receive_callback(uint8_t *data, size_t size);
static void dispatch_thread(void);

void LOG(const char *__restrict __format, ...);

const char* UART_NAME = "/dev/ttyUSB0";
static int _uart_fs = -1;
static mutex lock_m;
static mutex signal_m;
static condition_variable signal_cv;

int main()
{
    if (!init_uart(UART_NAME))
    {
        LOG("Failed to open '%s'. Make sure is does exist and is not opened by anyone else.", UART_NAME);
        return -1;
    }

    GatewayModuleInterface_init(&lock_uart, &write_uart, &signal_wait,
            &signal_set, &receive_callback, &LOG);

    thread t(dispatch_thread);

    struct
    {
        uint8_t band;
        uint8_t hwrev;
        uint8_t serial_number[12];
        uint8_t minor;
        uint8_t major;
    } version;

    // send some invalid command
    LOG("> send some invalid command");
    GatewayModuleInterface_sendCommandWaitAnswer((GATEWAY_MODULE_CMDS_t) 8,
            NULL, 0, NULL, 0);

    // send version request
    LOG("> send version request");
    if (GatewayModuleInterface_sendCommandWaitAnswer(GATEWAY_MODULE_CMD_VERSION,
            NULL, 0, (uint8_t*) &version, sizeof(version)))
    {
        LOG("Version, hwrev: %d, major: %d, minor: %d, band: %d", version.hwrev,
                version.major, version.minor, version.band);
        LOG(
                "Serial: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X",
                version.serial_number[0], version.serial_number[1],
                version.serial_number[2], version.serial_number[3],
                version.serial_number[4], version.serial_number[5],
                version.serial_number[6], version.serial_number[7],
                version.serial_number[8], version.serial_number[9],
                version.serial_number[10], version.serial_number[11]);
    }
    else
    {
        LOG("Failed to ready version");
    }

    // send receive nack, to trigger current message in rx queue to be replied (if any)
    LOG("> send receive nack, to trigger current message in rx queue to be replied (if any)");
    GatewayModuleInterface_sendAck(GATEWAY_MODULE_CMD_RECEIVE, false);

    t.join();

    return 0;
}

static bool init_uart(const char *uart)
{
    _uart_fs = open(uart, O_RDWR | O_NOCTTY); //Open in non blocking read/write mode
    if (_uart_fs == -1)
    {
        return false;
    }

    struct termios options;
    tcgetattr(_uart_fs, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(_uart_fs, TCIFLUSH);
    tcsetattr(_uart_fs, TCSANOW, &options);

    return true;
}

static void lock_uart(bool lock)
{
    if (lock)
    {
        LOG("Lock");
        lock_m.lock();
    }
    else
    {
        LOG("Unlock");
        lock_m.unlock();
    }
}

static bool write_uart(uint8_t *data, size_t size)
{
    if (size > 0)
    {
        size_t w = write(_uart_fs, data, size);
        if (w != size)
        {
            return false;
        }
    }
    return true;
}

static bool signal_wait(int timeout)
{
    LOG("Wait signal");
    unique_lock<mutex> lck(signal_m);
    return signal_cv.wait_for(lck, chrono::milliseconds(timeout))
            != cv_status::timeout;
}

static void signal_set(void)
{
    LOG("Set signal");
    unique_lock<mutex> lck(signal_m);
    signal_cv.notify_one();
}

static void receive_callback(uint8_t *data, size_t size)
{
    LOG("TODO: Handle received: %i", size);
}

static void dispatch_thread(void)
{
    char c;
    while (true)
    {
        int r = read(_uart_fs, &c, 1);
        if (r != 1)
        {
            LOG("Read returned code: %i", r);
            break;
        }
        GatewayModuleInterface_dispatch(c);
    }

    cout << "Thread exit." << endl;
}

void LOG(const char *__restrict __format, ...)
{
    va_list args;
    va_start(args, __format);
    vprintf(__format, args);
    printf("\r\n");
    va_end(args);
}

