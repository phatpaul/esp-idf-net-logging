# HTTP SSE Logging Server Example

This example demonstrates how to use the `net-logging` component to implement a HTTP Server-Sent Events (SSE) logging server.

This example also shows how to provide a customized html page.

For more information, see the [net-logging Documentation](../../README.md)

## Features

- Simple built-in HTTP server implementation can run along-side other services.
- Handles multiple client connections.
- Sends real-time log updates.
- Automatically restarts when network changes.

## How to use this example

### Hardware Required

* A development board with ESP32/ESP32-S2/ESP32-C3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming

### Configure the project

```
idf.py menuconfig
```
* Open the project configuration menu (`idf.py menuconfig`) to configure options.
  * Configure WiFi or Ethernet: -> `Example Connection Configuration`
  * Change options for this example: -> `Example Project Config`


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

### Test the example :

Open a browser and enter the IP address of the ESP32 in the address bar with port 8080.  I.e. `http://192.168.4.1:8080`

![Image](https://github.com/user-attachments/assets/15a45454-03c1-49be-a5fa-1e1328c24d89)


### Save logs to disk with python script
Run the python script `sse-client.py` to connect to the device and save logs to disk.
```sh
python3 sse-client.py 192.168.4.1

2025-05-15 16:44:58,504 +==========================+
2025-05-15 16:44:58,504 | HTTP SSE Logging Client  |
2025-05-15 16:44:58,504 +==========================+
2025-05-15 16:44:58,504 Connecting to 192.168.4.1:8080
2025-05-15 16:44:58,504 Logging started. Press Ctrl-C to stop.
2025-05-15 16:44:58,504 
2025-05-15 16:44:58,504 
```
The logs are shown in the terminal and also written to a txt file in the same directory. The log files are rotated before each run and when they reach 1MB.
The python script will automatically try to reconnect and resume logging if disconnected.

## Custom HTML Page
This example shows how you can customize the HTML page without modifying the source code in the library. 
* Set the option: `menuconfig` -> `Component config` -> `NET Logging` -> `Use a custom index.html asset for the built-in HTTP SSE Loggging Server`
* Place your custom html in a file located at `myProjectDir/www/index.html`
* FYI: Take a look at the CMakeLists.txt in this example project to see how it works.

## References

- [nopnop2002/esp-idf-net-logging](https://github.com/nopnop2002/esp-idf-net-logging)
- [ESP-IDF Example of non-blocking sockets](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/non_blocking)


## License

This example is provided under the MIT License. See the LICENSE file for details.
