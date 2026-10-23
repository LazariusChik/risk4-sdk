# Interfaces and current limits

This buildable SDK targets kernel ABI 26, loader ABI 5 and
manifest v5. Public headers and compiled RV32 libraries are identified by
`snapshot.json`. Runtime and graphics implementation sources remain private.

| Area | Included app-side interface | Limits |
| --- | --- | --- |
| Lifecycle | Loaded entry/runtime, normal return, readiness and exit hooks | Return from `proto_loaded_main` for runtime finalization; do not substitute a bare-metal entry point |
| Memory/workers | Allocation, registration, worker create/join/detach | Four process-owned workers; arguments must reference shared process memory rather than another thread's private stack; allocation can fail |
| UI/Game2D | Surface/command APIs plus the compiled `proto_gfx` library | Logical 320x240 game coordinates; internal sharp rendering does not change app coordinates; available commands depend on the runtime |
| Game3D | Public headers and compiled app-side helpers | Capability-gated software rendering; immutable uploaded resources, bounded queues and retirement; [details](game3d.md) |
| IPC/dialogs/objects | Message send/receive, trusted Open/Save dialogs, timer/GPIO and process-local shared-memory interfaces | Authority, live handles and registered buffers remain kernel-mediated; not unrestricted cross-process memory |
| Input | Key, pointer and gamepad queries | Pointer ABI 25, gamepad ABI 26; foreground rules and device/transport compatibility still apply |
| Audio | Playback open/write/tone/volume/flush/stat | Board codec/output routing needs validation; no microphone API is promised |
| Storage/resources | Mediated logical paths, file lifecycle and package resources | Mutating open flags require WRITE; `/self/` binds to kernel-owned identity; buffers/handles must remain live |
| TCP networking | Connect/listen/accept/read/write/poll/shutdown | No generic POSIX, UDP or TLS promise; ports, permissions, quotas and board radio transport are mediated |

One known header-comment discrepancy is retained in the exact source snapshot:
network commentary says eight global sockets, while the current broker allows
twelve. Six live sockets per process and eight port leases are separate limits;
the shared pool can be exhausted by other callers.

Use `proto_api_limits()` for runtime ABI/features. Current source advertises
`PROTO_FEATURE_FILE_LIFECYCLE`; reserved foundation names in the header are not
proof that synchronization, wait-many, durable replace, clocks, random, jobs or
pipes are implemented. A zero limit is unreported/unimplemented.

The `full` profile links the compiled high-level `proto_3d_*` helpers;
`core` deliberately omits them for apps that do not need 3D. The renderer and
game assets remain separate. Console/mechanical design is outside this SDK.
No ESP-IDF, FreeRTOS, FATFS or display-driver implementation is needed
by an app build; apps call the SDK/syscall interface.

U-mode/PMP, authenticated dispatch, capabilities, pointer ownership and live
handle generations are the intended boundary. This experimental prototype is
not independently security-certified. Secure Boot, flash encryption and rollback
protection are disabled in the development configuration; kernel and M-mode
SYSTEM applications remain trusted. No physical-attack protection is asserted.

Host build/ELF checks establish build completeness for the four examples only.
Panel support, frame rate, resource headroom, audio, wireless, fault recovery,
installation and LILYGO bring-up require named firmware/hardware validation.
