# Snapshot notes

## Engineering preview — 2026-10-22

- Animated README banner: original RISK4 logo, a pulsing white/orange RISK glow, and
  security/performance/hardware symbols in a seamless four-second GIF.

- Security, app isolation and hardware access lead the platform overview.
- GPIO and TCP are documented as available; I²C/SPI/UART app interfaces are
  clearly identified as roadmap items.
- Simplified the README and removed the contribution checklist and LILYGO
  status row from the landing page.

- App runtime, freestanding compiler helpers, retained Game2D and Game3D app SDK.
- Four examples build and package without the private OS checkout, including
  a commented app-side pentest with denied requests and an intentional fault.
- Both `core` and `full` profiles ship as stripped RV32 static libraries.
  Runtime and graphics implementation sources remain private.
- Expanded platform coverage documents 3D, the fully functional Store,
  workers/IPC, trusted file dialogs and other mediated services.
- Kernel ABI 26, loader ABI 5, manifest v5; RV32 IMAFC / ilp32f.
- Detached Windows/Python 3.11.2/GCC 14.2.0 example builds verified.
- Source hashes and base revision recorded in `snapshot.json`.
- Organized engineering review with SDK architecture, example walkthroughs,
  repository artwork and issue templates; snapshot bytes preserved across Git checkouts.
- Banner uses the existing RISK4 white/orange wordmark, with a high-resolution
  PNG for README rendering and editable SVG source.

Pending: SDK license terms, demonstration recordings, device execution and
LILYGO board integration. No firmware, kernel source or measured FPS result
is distributed with this snapshot.
