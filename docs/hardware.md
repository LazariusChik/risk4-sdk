# Hardware & connectivity

RISK4 is intended for building device utilities, sensor applications, peripheral
tools and hardware experiments alongside ordinary applications. Hardware access
goes through OS permissions and device handles so an app does not need to own
the entire firmware.

## Available now

**GPIO:** configure available pins as input/output, select pulls, read digital
levels and write outputs. Apps open `/dev/gpio/<pin>` with the required handle
rights and GPIO capability, then use SDK read/write/ioctl operations. The
kernel checks valid pins and protects pins reserved by the display, audio
and Wi-Fi transport drivers. Available pins depend on the board and wiring.

**TCP networking:** connect to remote services or listen for incoming
connections using mediated socket objects. Wi-Fi on supported reference
hardware uses an ESP32-C6 ESP-Hosted coprocessor. Networking requires the
appropriate permissions and working board/radio configuration.

**Input and audio:** key, pointer and gamepad interfaces, plus audio playback
streams and tones. These interfaces use the devices supported by the firmware;
they are not a generic USB or Bluetooth peripheral framework.

## Connectivity roadmap

The next hardware interfaces planned for applications are:

- **I²C** for sensors, displays and peripheral register access.
- **SPI** for external modules and higher-throughput peripherals.
- **UART** for serial terminals, device communication and diagnostic tools.

These are planned SDK interfaces, not shipped bus drivers or working examples
in this snapshot. Internal firmware drivers using a bus do not make that bus
available to installed applications. GPIO access alone does not establish a
timing-qualified I²C/SPI/UART implementation.

The intended model is permission-controlled bus access, with board-reserved
pins and peripherals protected by the OS. Bus transactions, pin assignment,
timeouts and ownership rules will be documented with the implementation.

## What to build

With current GPIO and TCP interfaces, applications can work with digital I/O
and networked devices within the board's supported configuration. The bus
roadmap expands that toward sensor dashboards, peripheral explorers, serial
consoles and hardware debugging utilities. Dedicated protocol analyzers,
logic capture and arbitrary radio tools are not supplied by this SDK.
