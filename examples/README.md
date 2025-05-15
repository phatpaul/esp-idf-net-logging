# Net-Logging Examples

This folder contains example applications and scripts demonstrating the usage of the `net-logging` component. These examples are designed to help developers understand how to integrate and utilize the net-logging functionality in their projects.

## Available Examples

### 1. **Basic Net-Logging Example**
- Demonstrates how to initialize and use the `net-logging` component for basic logging.
- You can enable/disable several simple networking protocol options via menuconfig.

### 2. **UART Select Net-Logging Example**
- Demonstrates when UART0 is used to communicate with some peripherals, it can't be used for logging. So logging is redirected to the network.
- You can enable/disable several simple networking protocol options via menuconfig.

### 3. **MQTT Net-Logging Example**
- Demonstrates publishing logs to an MQTT broker.
- TODO

### 3. **VFS File Logging Example**
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
