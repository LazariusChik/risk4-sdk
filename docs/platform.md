# Platform

RISK4 / ProtoOS is a kernel and application platform for building your own
ESP32-P4 devices. Hardware tools, connected appliances, custom interfaces and
consoles can share its native app model and security architecture. This
repository provides the app SDK; the kernel, drivers and graphical shell are
part of the separately supplied firmware.

## Isolation and permissions

Native applications run in RISC-V U-mode with PMP-constrained memory access.
The kernel authenticates calls, checks capabilities and permissions, and tracks
buffer ownership and live object handles. Threads have private stacks; data
shared between workers belongs in process-shared memory.

Apps use logical storage paths instead of raw filesystem access. Trusted
Open/Save dialogs grant access through the OS. Device and network operations
also go through kernel interfaces. See [architecture](architecture.md) and
[security](../SECURITY.md) for the trust boundary.

## Applications and system services

The SDK includes native app startup, allocation, logging, worker threads,
IPC, file/resource access, timers and process-local shared-memory interfaces.
An app can create up to four process-owned workers.

The graphical shell launches separately installable apps. The Store provides
a functional catalog, installation and updates. SDK bundle tools also support
development installs without a Store deployment.

## Hardware and connectivity

GPIO handles support configuration, digital reads and writes. TCP networking
supports outbound connections and listening sockets. Key, pointer, gamepad and
audio playback interfaces are available; physical devices and radio transport
depend on the firmware and board.

I²C, SPI and UART application APIs are on the roadmap. See
[hardware & connectivity](hardware.md) for what is available now and planned.

## Graphics

Game2D supplies retained drawing, sprites, text and scene helpers. Game3D
provides software 3D rendering with meshes, textures, lighting, materials and
HUD composition. Both app helper libraries are included; see the
[Game3D guide](game3d.md) and [examples](../examples/README.md).

## SDK status

I've verified compilation, ELF checks and packaging for all four examples
with the documented toolchain. Device checks are listed in the [build guide](build.md).
Runtime feature discovery determines which optional APIs a firmware provides;
reserved foundation declarations do not establish implementation. Current
foundation discovery reports `PROTO_FEATURE_FILE_LIFECYCLE`.

Detailed quotas and interface limits live in [interface notes](interface-notes.md).
