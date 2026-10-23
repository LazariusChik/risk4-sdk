# Devices you can build

RISK4 is a foundation for devices that run native apps and interact with the
outside world. Isolation is useful wherever an app handles data supplied by
another person, device or service.

| Device | External input | Why the kernel boundary matters |
| --- | --- | --- |
| Portable hardware/hacking tool | Network replies, imported files and attached-device data | A utility gets controlled access to files and hardware; a compromised utility cannot simply claim another app's identity or access protected memory |
| Communication terminal or radio front end | Messages, peer names and protocol metadata | Message-processing apps can run with separate storage and permissions; radio hardware and drivers require their own integration |
| IoT appliance or control panel | Remote commands, configuration files and service responses | Network-facing logic can be restricted to the device operations and storage it needs |
| App-restricted console or kiosk | Installed apps, game assets, save files and network content | Separate app identities, private data, signed-release admission and device permissions control what installed code can access |

## Why incoming data matters

A peer can send an oversized field, a file can contain a malformed record, or
a downloaded package can be altered. A parsing bug may turn that input into
unintended memory access or app execution. The kernel checks memory ownership,
caller identity and authority when that app requests system access.

That helps contain a compromised app; it does not repair the parser or decide
whether a valid-looking command from a peer is trustworthy. Applications still
need input validation, authentication and appropriate transport protection.
The [security overview](../SECURITY.md) gives concrete blocked-action examples.

GPIO and TCP interfaces are available now. I²C/SPI/UART app APIs are planned;
a general radio API is not included. See [hardware & connectivity](hardware.md).
Product firmware lockdown, including boot verification and physical-access
protections, is separate from application isolation.
