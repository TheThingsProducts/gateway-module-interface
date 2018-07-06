// Copyright © 2016-2018 The Things Products
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include <stddef.h>
#include "gateway-module-interface.h"

#define LOG(...) {if (g_log != NULL) {g_log(__VA_ARGS__);}}

#define FRAME_START 0x23
#define FRAME_CR    0x0D

typedef enum
{
    STATE_WAIT_FOR_START,
    STATE_WAIT_FOR_CMD,
    STATE_WAIT_FOR_LEN0,
    STATE_WAIT_FOR_LEN1,
    STATE_WAIT_FOR_DATA,
    STATE_WAIT_FOR_CHECKSUM,
    STATE_WAIT_FOR_CR
} STATE_t;

typedef enum
{
    RX_TYPE_ANSWER, RX_TYPE_INVALID, RX_TYPE_RECEIVE
} RX_TYPE_t;

typedef struct
{
    GATEWAY_MODULE_CMDS_t cmd;
    uint8_t cmd_sequence;
    uint8_t ans_sequence;
    uint8_t *ans_payload;
    size_t ans_payload_max_size;
    size_t ans_length;
} answer_context_t;

typedef struct
{
    GATEWAY_MODULE_CMDS_t cmd;
    uint8_t *payload;
    size_t max_size;
    size_t length;
    size_t counter;
    uint8_t checksum;
    uint8_t sequence;
} rx_context_t;

typedef struct
{
    uint8_t start;
    uint8_t cmd;
    uint8_t len0;
    uint8_t len1;
} header_t;

typedef struct
{
    uint8_t chksum;
    uint8_t stop;
} footer_t;

static void setState(STATE_t newState);
static size_t maxAnswerLength(GATEWAY_MODULE_CMDS_t cmd);

static gateway_module_interface_write_lock_t g_write_lock;
static gateway_module_interface_write_t g_write;
static gateway_module_signal_wait_t g_signal_wait;
static gateway_module_signal_set_t g_signal_set;
static gateway_module_receive_callback_t g_receive_callback;
static gateway_module_log_t g_log;
static answer_context_t g_pending_command;
static rx_context_t g_rx_context;
static STATE_t g_state;
static RX_TYPE_t g_rx_type;
static uint8_t g_receive_buffer[300];

void GatewayModuleInterface_init(
        gateway_module_interface_write_lock_t write_lock,
        gateway_module_interface_write_t write,
        gateway_module_signal_wait_t signal_wait,
        gateway_module_signal_set_t signal_set,
        gateway_module_receive_callback_t receive_callback,
        gateway_module_log_t log)
{
    g_write_lock = write_lock;
    g_write = write;
    g_signal_wait = signal_wait;
    g_signal_set = signal_set;
    g_receive_callback = receive_callback;
    g_log = log;
    setState(STATE_WAIT_FOR_START);
}

bool GatewayModuleInterface_sendCommandWaitAnswer(GATEWAY_MODULE_CMDS_t cmd, uint8_t *cmd_payload, size_t cmd_payload_size, uint8_t *ans_payload, size_t ans_payload_max_size)
{
    bool ret = true;

    g_write_lock(true);

    LOG("Command, cmd: 0x%02X, size: %d", cmd, cmd_payload_size);
    
    // set info for dispatcher
    g_pending_command.cmd = cmd;
    g_pending_command.ans_payload = ans_payload;
    g_pending_command.ans_payload_max_size = ans_payload_max_size;
    g_pending_command.ans_length = 0;

    // fill header
    header_t header;
    header.start = FRAME_START;
    header.cmd = (uint8_t)cmd;
    header.len0 = cmd_payload_size & 0xFF;
    header.len1 = (cmd_payload_size >> 8) & 0xFF;

    // fill footer (with checksum)
    footer_t footer;
    footer.chksum = 0;
    footer.stop = FRAME_CR;
    uint8_t *p = (uint8_t *)&header;
    size_t i;
    for (i = 0; i < sizeof(header_t); i++)
    {
        footer.chksum += p[i];
    }
    p = cmd_payload;
    for (i = 0; i < cmd_payload_size; i++)
    {
        footer.chksum += p[i];
    }

    ret = ret && g_write((uint8_t *)&header, sizeof(header));
    ret = ret && g_write(cmd_payload, cmd_payload_size);
    ret = ret && g_write((uint8_t *)&footer, sizeof(footer));
    if (ret)
    {
        while ((ret = g_signal_wait(1000)) == true)
        {
            if (g_pending_command.ans_sequence == g_pending_command.cmd_sequence)
            {
                break;
            }
        }
        if (!ret)
        {
            LOG("Timeout on cmd: 0x%02X", cmd);
        }
    }
    // set info for dispatcher
    g_pending_command.cmd_sequence++;
    g_pending_command.cmd = GATEWAY_MODULE_CMD_NONE;
    g_pending_command.ans_payload = NULL;
    g_pending_command.ans_payload_max_size = 0;
    g_pending_command.ans_length = 0;

    g_write_lock(false);

    return ret;
}

bool GatewayModuleInterface_sendCommandWaitAck(GATEWAY_MODULE_CMDS_t cmd, uint8_t *cmd_payload, size_t cmd_payload_size)
{
    bool ret = true;
    uint8_t ack;

    ret = ret && GatewayModuleInterface_sendCommandWaitAnswer(cmd, cmd_payload, cmd_payload_size, &ack, 1);
    ret = ret && (ack == 0); // Extra check if ACK is '0'

    return ret;
}

void GatewayModuleInterface_sendAck(GATEWAY_MODULE_CMDS_t cmd, bool ack)
{
    bool ret = true;

    g_write_lock(true);

    // fill header
    header_t header;
    header.start = FRAME_START;
    header.cmd = (uint8_t)cmd;
    header.len0 = 1;
    header.len1 = 0;
    uint8_t data = ack ? 0 : 1;
    // fill footer (with checksum)
    footer_t footer;
    footer.chksum = 0;
    footer.stop = FRAME_CR;
    uint8_t *p = (uint8_t *)&header;
    size_t i;
    for (i = 0; i < sizeof(header_t); i++)
    {
        footer.chksum += p[i];
    }
    footer.chksum += data;

    ret = ret && g_write((uint8_t *)&header, sizeof(header));
    ret = ret && g_write(&data, 1);
    ret = ret && g_write((uint8_t *)&footer, sizeof(footer));

    g_write_lock(false);
}

void GatewayModuleInterface_dispatch(uint8_t d)
{
    switch (g_state)
    {
    case STATE_WAIT_FOR_START:
        if (d == FRAME_START)
        {
            g_rx_context.checksum = FRAME_START;
            setState(STATE_WAIT_FOR_CMD);
        }
        break;

    case STATE_WAIT_FOR_CMD:
        g_rx_context.checksum += d;
        g_rx_context.cmd = (GATEWAY_MODULE_CMDS_t)d;
        if (g_rx_context.cmd == g_pending_command.cmd)
        {
            g_rx_type = RX_TYPE_ANSWER;
            g_rx_context.payload = g_pending_command.ans_payload;
            g_rx_context.max_size = g_pending_command.ans_payload_max_size;
            g_pending_command.ans_sequence = g_pending_command.cmd_sequence;
            setState(STATE_WAIT_FOR_LEN0);
        }
        else if (g_rx_context.cmd == GATEWAY_MODULE_CMD_RECEIVE)
        {
            g_rx_type = RX_TYPE_RECEIVE;
            g_rx_context.payload = g_receive_buffer;
            g_rx_context.max_size = sizeof(g_receive_buffer);
            setState(STATE_WAIT_FOR_LEN0);
        }
        else if (g_rx_context.cmd == GATEWAY_MODULE_CMD_INVALID)
        {
            g_rx_type = RX_TYPE_INVALID;
            g_rx_context.payload = NULL;
            g_rx_context.max_size = 0;
            setState(STATE_WAIT_FOR_LEN0);
        }
        else
        {
            LOG("Receiving unknown data");
            setState(STATE_WAIT_FOR_START);
        }
        break;

    case STATE_WAIT_FOR_LEN0:
        g_rx_context.checksum += d;
        g_rx_context.length = d;
        setState(STATE_WAIT_FOR_LEN1);
        break;

    case STATE_WAIT_FOR_LEN1:
        g_rx_context.checksum += d;
        g_rx_context.length += ((int)d) << 8;
        // Sanity check on length
        if (g_rx_context.length <= maxAnswerLength(g_rx_context.cmd))
        {
            g_rx_context.counter = 0;
            setState(STATE_WAIT_FOR_DATA);
        }
        else
        {
            LOG("Received length %d too large for cmd 0x%02X", g_rx_context.length, g_rx_context.cmd);
            setState(STATE_WAIT_FOR_START);
        }
        break;
    case STATE_WAIT_FOR_DATA:
        g_rx_context.checksum += d;
        if (g_rx_context.counter < g_rx_context.max_size)
        {
            g_rx_context.payload[g_rx_context.counter] = d;
        }
        g_rx_context.counter++;
        if (g_rx_context.counter >= g_rx_context.length)
        {
            setState(STATE_WAIT_FOR_CHECKSUM);
        }
        break;

    case STATE_WAIT_FOR_CHECKSUM:
        if (g_rx_context.checksum == d)
        {
            setState(STATE_WAIT_FOR_CR);
        }
        else
        {
            LOG("Invalid checksum: 0x%02X, calculated: 0x%02X", d, g_rx_context.checksum);
            GatewayModuleInterface_sendAck(g_rx_context.cmd, false);
            setState(STATE_WAIT_FOR_START);
        }
        break;

    case STATE_WAIT_FOR_CR:
        if (d == FRAME_CR)
        {
            if (g_rx_type == RX_TYPE_ANSWER)
            {
                if (g_pending_command.ans_sequence == g_pending_command.cmd_sequence)
                {
                    LOG("Answer, cmd: 0x%02X, size: %d", g_rx_context.cmd, g_rx_context.length );
                    // only signal when sequence number agree (could mismatch on timeout)
                    g_pending_command.ans_length = g_rx_context.length;
                    g_signal_set();
                }
                else
                {
                    LOG("Answer, rec: %d, exp: %d", g_pending_command.ans_sequence, g_pending_command.cmd_sequence);
                }
            }
            else if (g_rx_type == RX_TYPE_INVALID)
            {
                LOG("Ans: Invalid");
                g_pending_command.ans_length = 0;
                g_signal_set();
            }
            else if (g_rx_type == RX_TYPE_RECEIVE)
            {
                g_receive_callback(g_receive_buffer, g_rx_context.length);
            }
        }
        else
        {
            LOG("No correct stop 0x%02X:, expected: 0x%02X", d, FRAME_CR);
        }
        setState(STATE_WAIT_FOR_START);
        break;
    }
}

static void setState(STATE_t newState)
{
    //LOG("Switch state %d->%d", _state, newState);
    g_state = newState;
}

static size_t maxAnswerLength(GATEWAY_MODULE_CMDS_t cmd)
{
    size_t max_ret_size;
    switch (cmd)
    {
    case GATEWAY_MODULE_CMD_SAVE:
    case GATEWAY_MODULE_CMD_SETUART:
    case GATEWAY_MODULE_CMD_START:
    case GATEWAY_MODULE_CMD_STOP:
    case GATEWAY_MODULE_CMD_SEND:
    case GATEWAY_MODULE_CMD_RFCONFIG:
    case GATEWAY_MODULE_CMD_IFCONFIG:
    case GATEWAY_MODULE_CMD_IF8CONFIG:
    case GATEWAY_MODULE_CMD_IF9CONFIG:
    case GATEWAY_MODULE_CMD_TXABORT:
    case GATEWAY_MODULE_CMD_TXSTATUS:
    case GATEWAY_MODULE_CMD_SETLEDS:
    case GATEWAY_MODULE_CMD_SETSYNC:
    case GATEWAY_MODULE_CMD_GETSYNC:
    case GATEWAY_MODULE_CMD_RXSTATUS:
    case GATEWAY_MODULE_CMD_SENDCW:
    case GATEWAY_MODULE_CMD_INVALID:
    case GATEWAY_MODULE_CMD_MFGDATA:
    case GATEWAY_MODULE_CMD_BOOTLOADER_MODE:
        max_ret_size = 1;
        break;

    case GATEWAY_MODULE_CMD_GETUART:
        max_ret_size = 4;
        break;

    case GATEWAY_MODULE_CMD_RFCHAIN:
        max_ret_size = 5;
        break;

    case GATEWAY_MODULE_CMD_IFCHAIN:
        max_ret_size = 7;
        break;

    case GATEWAY_MODULE_CMD_IF8CHAIN:
        max_ret_size = 8;
        break;

    case GATEWAY_MODULE_CMD_VERSION:
        max_ret_size = 16;
        break;

    case GATEWAY_MODULE_CMD_IF9CHAIN:
        max_ret_size = 11;
        break;

    case GATEWAY_MODULE_CMD_RECEIVE:
        max_ret_size = 300;
        break;

    case GATEWAY_MODULE_CMD_RESET:
    default:
        max_ret_size = 0;
        break;
    }
    return max_ret_size;
}
