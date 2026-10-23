# Building and running applications

Run commands from the SDK root. No private repository, ESP-IDF framework,
FreeRTOS headers, firmware build or hardware is needed to build these examples.

## Toolchain

Use Python 3.10+ and Espressif's `riscv32-esp-elf-gcc` with its matching binutils.
The tested compiler is GCC 14.2.0 from `esp-14.2.0_20260121`. Activate the
ESP-IDF installation's `export.ps1` or `export.sh`, or pass `--gcc` with an
absolute compiler path. Build-only commands use Python's standard library.
Other compiler versions are unvalidated by this snapshot.

```text
python build_examples.py
```

The helper runs the same public tools and checks used by direct builds. It
returns failure if compilation, linking, bundle validation or the ELF
rejection selftests fail. Its report is local build evidence, not device proof.

## Direct build

```text
python tools/protelf_dev.py build-bundle --sdk-profile core --app examples/hello --app-id 7001 --name hello --package dev.proto.sdk.hello --out build/hello.pelf
python tools/protelf_dev.py build-bundle --sdk-profile core --app examples/hello2d --app-id 7002 --name hello2d --package dev.proto.sdk.hello2d --heap 16384 --out build/hello2d.pelf
python tools/protelf_dev.py build-bundle --sdk-profile full --app examples/hello3d --app-id 7003 --name hello3d --package dev.proto.sdk.hello3d --heap 65536 --out build/hello3d.pelf
python tools/protelf_dev.py build-bundle --sdk-profile core --app examples/pentest --app-id 7004 --name pentest-demo --package dev.proto.sdk.pentest --heap 4096 --out build/pentest.pelf
python tools/protelf_dev.py inspect --elf build/protelf_hello2d/app.elf --dynamic
python tools/protelf_dev.py selftest --elf build/protelf_hello2d/app.elf
```

Use `--sdk-profile core` for the loaded runtime and Game2D, or `--sdk-profile
full` to add the Game3D app helpers. Both profiles are included; `full` is the
default for `build`, `build-bundle` and `deploy`. Missing or altered profile libraries are
reported as errors rather than silently substituting another profile.

Apps include `proto_loaded.h`; Game2D apps also include `proto_gfx.h`, and
Game3D apps include `proto_3d.h`. The
builder supplies include paths and compiles `.c` and `.S` files directly inside
the selected app directory. Copy an example directory and choose a distinct
app id, name and package. Package names are 1..47 lowercase ASCII bytes using
letters, digits, hyphens and dots; dot-separated labels must be nonempty and
the whole name must contain a non-digit (INV-051).

The tool infers permissions/capabilities from recognized SDK calls. This is a
convenience scanner, not a compiler analysis or an authorization boundary:
inspect the manifest output when using custom wrappers. Installation approval
and runtime kernel checks still determine authority. No permission bypass is
introduced by the core library profile.

The default loader ABI 5 linker script uses a fixed `0x08000000` RX-to-RW bias
for flash XIP. Do not replace it with an ordinary bare-metal linker script or
link against ESP-IDF/FreeRTOS directly. The provided ABI 4 and library scripts
are included for the tools' compatibility paths; this release verifies the
four ABI 5 examples only. SIMD assembly, resource-bearing apps, dynamic libraries
and serial deployment are not acceptance-tested by this release.

## Device prerequisites and expected behavior

For an evaluation build, let me know your board model and P4 revision.
I'll provide matching firmware and installation instructions. P4 v1/v3 images
differ; `.pelf` app bundles are installed within ProtoOS, not flashed as firmware.

After logging into the device, transfer the development bundle using the
instructions supplied with the firmware and open it in the ProtoELF installer.
Unsigned installs must be enabled by the firmware's development policy.
For a bundle already at the following path, use these terminal commands:

```text
install-bundle /shared/downloads/hello.pelf
run hello
```

Hello should log `PASS SDK hello: allocated, freed, returning normally` at WARN
level and exit. Hello 2D should show a blue card with white text for two seconds
before returning to the shell. Its log says only that submission was accepted;
visual presentation must be checked on the actual device. A dropped frame is
reported as a failed example rather than counted as successful presentation.
Hello 3D should show an orange cube, with lighting when supported. It waits for
the OS's `PRESENTED` status, then holds the scene for two seconds and retires
resources. A failed or timed-out presentation is reported as failure.
None of these examples has been run on hardware for this SDK release.

The [Pentest demo](../examples/pentest/README.md) first checks returned denials,
then deliberately attempts a privileged CSR read. Launch it with
`run pentest-demo`; the kernel should report its fault and reap that app.
`build_examples.py` only builds and checks the bundles; it does not run probes.

Serial commands require `python -m pip install pyserial`, exclusive access to
the device port and a logged-in ProtoOS terminal. `deploy --help` describes the
available options; serial transfer/install/recovery remain device checks.
Release signing additionally requires `cryptography` and your release signing
keys. Signing keys aren't included in the SDK.
