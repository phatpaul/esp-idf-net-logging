# esp-idf-net-logging
Redirect ESP-IDF logs to multiple sources.

Forked from [nopnop2002/esp-idf-net-logging](https://github.com/nopnop2002/esp-idf-net-logging) and refactored to be more modular and robust.


### Motivation
esp-idf has a Logging library.   
The Logging library contains the ```esp_log_set_vprintf``` function.   
By default, log output goes to UART0.    
This component can be used to redirect log output to some other destination, such as file or network.    



## Requirements
- ESP-IDF V4.3 or later.   
 
## Build the example projects
There are examples of several protocols for sending logs. 
- HTTP Server (Using Server-Sent-Events, SSE)
- UDP Client (broadcast and unicast)
- Multicast IP
- TCP Client
- MQTT Client (Publish)
- HTTP Client (POST)

See instructions in the [examples folder](./examples/)

### TODO: Some ideas for more logging methods
- BLE (Bluetooth LE) Log Sender
  * This method could be useful for troubleshooting IP network issues, since BLE stack is completely separate from the TCP/IP stack.
  * Will need a companion host app to recieve and save the logs. I.e. Android/iOS App (or an app on a PC that has a BLE adapter)
- Local file logging.
  * option to save the logs to a local file (i.e. using VFS), so the logs are actually written to an internal filesystem such as FAT, SPIFFS, LittleFS, etc.
  * I.e. it could be stored on a SD card and the card removed to retrieve the file.
Or store it to internal flash and download it from an embedded HTTP server.
  * See https://github.com/nopnop2002/esp-idf-net-logging/issues/7

## Configuration   
The library can be configured via `idf.py menuconfig`.


### Use xRingBuffer as IPC
* Set the option: `menuconfig` -> `Component config` -> `NET Logging` -> `Use xRingBuffer as IPC`

* Both xMessageBuffer and xRingBuffer are interprocess communication (IPC) components provided by ESP-IDF. Several drivers provided by ESP-IDF use xRingBuffer. This project uses xMessageBuffer by default. If you use this project at the same time as a driver that uses xRingBuffer, using xRingBuffer uses less memory. Memory usage status can be checked with ```idf.py size-files```.   

## Add this component to your own project

### (Option 1 - easy) using idf component manager 
Create idf_component.yml in the same directory as main.c.   
```
YourProject --+-- CMakeLists.txt
              +-- main --+-- main.c
                         +-- CMakeLists.txt
                         +-- idf_component.yml
```

Contents of idf_component.yml.
```
dependencies:
  net-logging:
    git: https://github.com/nopnop2002/esp-idf-net-logging.git
```

When you build a projects esp-idf will automaticly fetch repository to managed_components dir and link with your code.   
```
YourProject/
              -- CMakeLists.txt
              +-- main/ 
              |          -- main.c
              |          -- CMakeLists.txt
              +-- managed_components/
                          +-- net-logging/
```

### (Option 2 - advanced) as a git submodule 

This allows you to pin an exact version of this component at an exact commit for repeatable builds. It is recommended for developers that plan to modify and contribute to this component.

In your project, add this as a git submodule to your components/ directory.
```sh
git submodule add https://github.com/nopnop2002/esp-idf-net-logging.git components/net-logging
git submodule update --init --recursive
```
It should look this this:
```
YourProject/
              -- CMakeLists.txt
              +-- main/ 
              |          -- main.c
              |          -- CMakeLists.txt
              +-- components/
                          +-- net-logging/
                                         +-- .git/  ( submodule repository )
                                         (... rest of this component source files)
```



## API   
Initialize the core net-logging component first. Choose whether to continue outputting logs to the default stdout (UART).
```c
/**
 * @brief Initialize the netlogging component. Must do this before using any other functions.
 * @param enableStdout If true, log entries will also be printed to stdout (UART).
 * @return ESP_OK on success, ESP_ERR_NO_MEM if memory allocation failed.
 */
esp_err_t netlogging_init(bool enableStdout);
```

Use one of the built-in protocols. It is possible to use multiple protocols simultaneously. I.e. to broadcast them out in UDP packets and also serve logs via a built-in HTTP server:

```c
// Init and start built-in UDP client log sender
udp_logging_param_t udp_logging_params = NETLOGGING_UDP_DEFAULT_CONFIG();
udp_logging_params.ipv4addr = "255.255.255.255";
udp_logging_params.port = 6789;
netlogging_udp_client_init(&udp_logging_params);
netlogging_udp_client_run();

// Init and start built-in HTTP-SSE log server
sse_logging_param_t sse_logging_params = NETLOGGING_SSE_DEFAULT_CONFIG();
sse_logging_params.port = 8080;
netlogging_sse_server_init(&sse_logging_params);
netlogging_sse_server_run();
```

### (Advanced usage) To roll-your-own log handler, create a buffer and register it with the net-logging component.
```c
/**
 * @brief Register a buffer to be used for logging.
 *
 * @param buffer The buffer to register. It must be a valid pointer to a MessageBufferHandle_t or RingbufHandle_t.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if buffer is NULL, ESP_ERR_NO_MEM if no empty slot found.
 */
esp_err_t netlogging_register_recieveBuffer(void *buffer);
```
Do some stuff with the log messages that come in to the buffer.



### (Optional) Cleanup if net-logging is no longer needed. 

Stop any built-in protocols that were started.
```c
// Stop and deinit built-in UDP client log sender
netlogging_udp_client_stop();
netlogging_udp_client_deinit();

// Stop and deinit built-in HTTP-SSE log server
netlogging_sse_server_stop();
netlogging_sse_server_deinit();
```

(Advanced usage) unregister any other buffers that were added.
```c
/**
 * @brief Unregister a buffer to be used for logging.
 *
 * @param buffer The buffer to unregister. Note that this function does not free the buffer itself, it just unregisters it from the logging system.
 * It is the caller's responsibility to free the buffer if it was dynamically allocated.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if buffer is NULL, ESP_ERR_NOT_FOUND if buffer was not found.
 */
esp_err_t netlogging_unregister_recieveBuffer(void *buffer)
```
Deinit the core net-logging component. The original logging vprintf will be restored (i.e. stdout). 
```c
/**
 * @brief Deinitialize the netlogging component. 
 * This should be called when the component is no longer needed to free up resources. 
 * Make sure to unregister all buffers before calling this function.
 * @return ESP_OK on success.
 */
esp_err_t netlogging_deinit()
```



## References

- [nopnop2002/esp-idf-net-logging](https://github.com/nopnop2002/esp-idf-net-logging)
- https://github.com/MalteJ/embedded-esp32-component-udp_logging
