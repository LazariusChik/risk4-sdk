# Proposed LILYGO engineering evaluation

Goal: bring RISK4's native applications, isolation and hardware interfaces to an
agreed LILYGO ESP32-P4 board as a foundation for hardware tools, connected
devices and custom interfaces. Graphics and games are additional use cases.
**Status: proposed; no LILYGO board acceptance
is recorded by this package.** This is a software port discussion, separate from
console product or mechanical design.

## Information to exchange before the port

| Board detail | Why it matters |
| --- | --- |
| Exact model, board revision, P4 silicon revision | Select compatible firmware memory map, instruction set and PMP setup |
| Flash and PSRAM sizes, modes and reserved regions | Size kernel/app storage, XIP arena and working heaps |
| Panel controller, bus, timing, orientation, backlight pins | Implement scanout, scaling and initialization |
| Touch/input controllers, interrupt pins and existing driver examples | Establish the device's input path and app focus behavior |
| Audio codec, I2S wiring, amplifier enables, sample rates | Validate playback routing and supported formats |
| Radio chip, SDIO/SPI wiring, firmware and ESP-Hosted compatibility | Validate network bring-up; do not assume 5 GHz |
| USB/UART routing, boot straps and recovery procedure | Establish flash, log capture, keyboard/input and recovery access |
| Known-working vendor ESP-IDF example and schematic | Compare initialization and isolate board-specific problems |

Current internal firmware uses patched ESP-IDF 5.5.4 and separate profiles for
legacy P4 v1 and Waveshare P4 v3 hardware. Those profiles are integration
references, not configuration files for a T-Display P4.

## Proposed acceptance sequence

1. Confirm board identity and unmodified vendor demonstration; record wiring,
   power supply, radio firmware and tool versions.
2. Boot the matching ProtoOS firmware variant and record isolation diagnostics,
   flash/RAM headroom and recovery access.
3. Validate the panel, orientation, surface sizes and input focus on the actual
   hardware; exercise repeated surface open/close and return to the shell.
4. Validate audio playback, input devices, storage and network behavior
   independently, including missing/disconnected peripherals.
5. Use the Store to install and update a native application, then launch, close
   and uninstall it without reflashing the OS. Verify saved data behavior and
   denied permission handling.
6. Exercise agreed GPIO/network utilities, permission checks and app fault
   containment, then Game2D/Game3D examples where relevant. Capture results
   with firmware commit, configuration, clocks, workload, repetitions and
   correctness evidence. Label failures and untested cases explicitly.
7. Provide an evaluation image, exact install/recovery instructions and a
   short device demonstration after acceptance checks are complete.

Schedule should be confirmed after the exact board and driver dependencies are
known. This package does not convert the earlier one-week estimate into a
validated delivery commitment.

## Review feedback requested

- Which board model/revision should be the first target?
- Can the team share the schematic and working display/touch/audio/radio examples?
- Which integration questions need a private source review?
- Which acceptance results would make this useful for LILYGO's evaluation?
