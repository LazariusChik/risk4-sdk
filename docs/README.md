# Documentation

Start with the [build guide](build.md). It takes the source examples to validated
`.pelf` app bundles without firmware sources or a connected board.

| Document | Read it for |
| :--- | :--- |
| [Build and run](build.md) | Toolchain, one-command build, direct commands and device prerequisites |
| [Platform](platform.md) | Isolation, native apps, hardware and system services |
| [Device use cases](use-cases.md) | Hardware tools, communications, appliances and consoles handling external input |
| [Security](../SECURITY.md) | Isolation, micro-ASLR, canaries, permissions and attack examples |
| [Pentest example](../examples/pentest/README.md) | Commented denial checks and a deliberate U-mode privilege fault |
| [Hardware & connectivity](hardware.md) | GPIO, networking and the I²C/SPI/UART roadmap |
| [Game3D](game3d.md) | Software renderer features, app helpers, limits and resource lifecycle |
| [Interfaces and limits](interface-notes.md) | Available SDK surfaces, runtime discovery and current limitations |
| [Architecture](architecture.md) | How app code, the SDK and the separately supplied OS fit together |
| [Demonstrations](media.md) | System walkthrough, Minecraft server and earlier demos |
| [Hardware evaluation](evaluation.md) | LILYGO port proposal, board details and integration checks |
| [Distribution terms](distribution.md) | SDK licensing status and the closed-source core boundary |

For code, browse the [examples](../examples/README.md). The root
[snapshot manifest](../snapshot.json) identifies the distributed files and their SHA-256 hashes.
