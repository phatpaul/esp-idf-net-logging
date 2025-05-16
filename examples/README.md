# Net-Logging Examples

This folder contains example applications and scripts demonstrating the usage of the `net-logging` component. These examples are designed to help developers understand how to integrate and utilize the net-logging functionality in their projects.

## Available Examples

### **Basic Net-Logging Example**
- Demonstrates how to initialize and use the `net-logging` component for basic logging.
- You can enable/disable several built-in networking protocol options via menuconfig.

### **HTTP SSE Logging Server Example**
- Demonstrates how to use the `net-logging` component to implement a HTTP Server-Sent Events (SSE) logging server.
- This example also shows how to provide a customized html page.

### **UART Select Net-Logging Example**
- Demonstrates when UART0 is used to communicate with some peripherals, it can't be used for logging. So logging is redirected to the network.
- You can enable/disable several built-in networking protocol options via menuconfig.

### **MQTT Net-Logging Example**
- Demonstrates publishing logs to an MQTT broker.
- TODO

### **VFS File Logging Example**
- Demonstrates writing logs to a local filesystem (internal FLASH or SD/MMC card).
- Saved logs can be later retrieved via an embedded HTTP server.
- TODO

## How to Run the Examples

1. Navigate to the example's directory.
2. Follow the instructions provided in example's Readme.

## Contributing

If you have additional examples or improvements, feel free to contribute by submitting a pull request.

## License

This project and examples are licensed under the terms of the [MIT License](../LICENSE).
