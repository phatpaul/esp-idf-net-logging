# esp-idf-net-logging
Redirect ESP-IDF logs to multiple sources.

Forked from [nopnop2002/esp-idf-net-logging](https://github.com/nopnop2002/esp-idf-net-logging) and refactored to be more modular and robust.



esp-idf has a Logging library.   
The Logging library contains the ```esp_log_set_vprintf``` function.   
By default, log output goes to UART0.    
This component can be used to redirect log output to some other destination, such as file or network.    

The following protocols are available for sending logs. See the examples for more information.
- HTTP Server (SSE)
- UDP Client (broadcast and unicast)
- Multicast IP
- TCP Client
- MQTT Client
- HTTP Client (POST)

## Requirements
- ESP-IDF V4.3 or later.   
 
## Build the example projects
See instructions in the [examples folder](./examples/)

## Configuration   
The library can be configured via `idf.py menuconfig`.


### Use xRingBuffer as IPC
* Set the option: `menuconfig` -> `Component config` -> `NET Logging` -> `Use xRingBuffer as IPC`

Both xMessageBuffer and xRingBuffer are interprocess communication (IPC) components provided by ESP-IDF.   
Several drivers provided by ESP-IDF use xRingBuffer.   
This project uses xMessageBuffer by default.   
If you use this project at the same time as a driver that uses xRingBuffer, using xRingBuffer uses less memory.   
Memory usage status can be checked with ```idf.py size-files```.   

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
Use one of the following.   
Subsequent logging will be redirected.   
```
esp_err_t udp_logging_init(char *ipaddr, unsigned long port, int16_t enableStdout);
esp_err_t tcp_logging_init(char *ipaddr, unsigned long port, int16_t enableStdout);
esp_err_t mqtt_logging_init(char *url, char *topic, int16_t enableStdout);
esp_err_t http_logging_init(char *url, int16_t enableStdout);
esp_err_t sse_logging_init(unsigned long port, int16_t enableStdout);
```

It is possible to use multiple protocols simultaneously.   
The following example uses UDP and SSE together.   
```
udp_logging_init("255.255.255.255", 6789, true);
sse_logging_init(8080, true);
```

## References

- [nopnop2002/esp-idf-net-logging](https://github.com/nopnop2002/esp-idf-net-logging)
- https://github.com/MalteJ/embedded-esp32-component-udp_logging
