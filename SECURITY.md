# Security architecture

RISK4 puts a kernel-controlled boundary between native applications and the
hardware and services they use.

- **U-mode and PMP:** apps run without machine-mode privileges. Physical Memory
  Protection restricts their memory access; each worker has a private stack.
- **Write XOR execute:** executable code and writable memory are separated.
  App heaps and stacks are not executable.
- **Authenticated syscalls:** requests are bound to the live caller. The kernel
  checks capabilities, permissions and buffer ownership before acting.
- **Owned handles:** files, devices and registered buffers are accessed through
  rights-checked handles. Generation checks reject stale references after reuse.
- **Private storage and controlled hardware:** the kernel resolves app-private
  paths using the caller's identity and mediates access to devices and GPIO.
- **Trusted UI:** permission prompts and Open/Save dialogs are owned by the
  kernel, including their input handling and the access they grant.
- **Release admission:** signed release bundles are checked for authenticity,
  compatibility and version policy. Development installs use a separate policy.
- **Controlled teardown:** process memory and resources stay live until active
  execution and outstanding users have quiesced, including across both cores.

## Memory hardening

- **Micro-ASLR:** bounded random offsets vary stack layout; randomized heap
  padding also reduces layout predictability.
- **Stack canaries:** compiler stack protection detects overwritten canaries
  in protected kernel and SYSTEM/M-mode frames. U-mode stacks use PMP window
  boundaries and are not compiler-canary instrumented.
- **Heap canaries and audits:** guards around secure-heap allocations and
  allocator integrity checks detect corruption when checked. Heap violations
  can quarantine the process heap and trigger process termination.
- **Metadata integrity:** process security metadata carries a keyed integrity
  seal; allocation descriptors also have integrity checks to detect tampering.

Randomization makes memory layout less predictable; canaries and integrity
checks detect corruption. PMP and access checks enforce the isolation boundary.

## Examples of attacks the kernel blocks

These are application actions covered by existing kernel checks:

| Attempt | Enforced boundary |
| --- | --- |
| Forge another app's PID or tamper with syscall credentials | Requests must authenticate as the live caller |
| Pass a protected kernel address or another worker's private stack as a syscall buffer | Pointer ownership, range and access checks |
| Jump into code placed in an app's heap or stack | PMP permissions keep writable data non-executable |
| Reuse a closed handle or an unregistered buffer ID | Live-owner and generation checks reject stale references |
| Open GPIO without permission or drive pins reserved for system hardware | GPIO capability, handle rights and reserved-pin checks |
| Read another app's private files without authority | Kernel-resolved storage namespaces and access policy |
| Modify a signed release bundle and install it as the original release | Signature and package-integrity checks at admission |

For example, a malformed network message might exploit a bug in a device
utility. If that app then attempts these actions, the same kernel checks
apply. Input validation and peer authentication still belong in the app;
a compromised app can misuse permissions it already holds.

See [device use cases](docs/use-cases.md) for where these boundaries are useful.

The commented [Pentest example](examples/pentest/README.md) demonstrates
returned denials and an intentional privilege fault that should terminate the app.

These protections address application isolation. The kernel, machine-mode
SYSTEM apps and firmware remain trusted. Development firmware does not enable
Secure Boot, flash encryption or hardware rollback protection.

## Reporting an issue

For security reports, open an issue asking me for a private contact without
posting exploit details. I'll arrange a way to share them privately.
