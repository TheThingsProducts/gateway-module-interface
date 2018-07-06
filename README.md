# Gateway Module Interface

This library is for coding and decoding UART frames for the LG8271/LG9271 modules. It is written for [The Things Gateway](https://github.com/TheThingsProducts/gateway/).

## Platform dependency
The platform dependency is decoupled by a small set of functions to be implemented per platform.

Mutex for the UART write action
```C
typedef void (*gateway_module_interface_write_lock_t)(bool lock);
```
The write to UART function
```C
typedef bool (*gateway_module_interface_write_t)(uint8_t *data, size_t size);
```

Wait for an semaphore to wait for an answer
```C
typedef bool (*gateway_module_signal_wait_t)(int timeout);
```

Signal of the semaphore to indicate an answer has received
```C
typedef void (*gateway_module_signal_set_t)(void);
```
	
Callback for a RECEIVE message (LoRa messages has been received)
```C
typedef void (*gateway_module_receive_callback_t)(uint8_t *data, size_t size);
```

Log function to print some debug information
```C
typedef void (*gateway_module_log_t) (const char * format, ...);
```

## Test application

It contains a small test application.

It is built and tested with Eclipse Oxygen.2 Release (4.7.2) and GCC 5.4.0 on Linux.

### Example output

Running the test application gives the following output:
```
> send some invalid command
Lock
Command, cmd: 0x08, size: 0
Wait signal
Ans: Invalid
Set signal
Unlock
> send version request
Lock
Command, cmd: 0x3A, size: 0
Wait signal
Answer, cmd: 0x3A, size: 16
Set signal
Unlock
Version, hwrev: 1, major: 1, minor: 4, band: 1
Serial: 4C-47-38-34-35-31-36-30-31-31-31-32
> send receive nack, to trigger current message in rx queue to be replied (if any)
Lock
Unlock
TODO: Handle received: 231
```