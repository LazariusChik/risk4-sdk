#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTO_ABI_VERSION 26u
#define PROTO_ABI_TYPES_DEFINED 1

// ------------------------------------------------------------------
// Stable user/kernel ABI V1
// ------------------------------------------------------------------
// This header is safe for sandboxed user programs. It contains only scalar
// values and by-value message structs. It intentionally exposes no FreeRTOS
// handles, process registry structs, kernel queues, or protected arena data.

#define LOGGER_PID 3u
#define TERMINAL_PID 4u

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
} LogLevel_t;

#define LOG_MSG_TYPE_BASE 100u

#define IPC_PAYLOAD_SIZE      128u
#define DEFAULT_MAILBOX_DEPTH  8u
#define LOGGER_MAILBOX_DEPTH  32u
#define KERNEL_RESPONSE_QUEUE_DEPTH 4u
#define KERNEL_API_QUEUE_DEPTH 10u
#define PROTO_PATH_MAX 96u
#define PROTO_NAME_MAX 48u
#define PROTO_ELF_NAME_MAX 32u
#define PROTO_ELF_PACKAGE_MAX 48u
#define PROTO_ELF_HASH_BYTES 32u
/* Largest flash-XIP arena span a manifest may request, as a slot-count
 * exponent. 4 => 16 slots => 2 MiB RX paired with 2 MiB RW, the
 * largest aligned span that fits the current 31-slot arena (see
 * protelf_xip.h). Raising this without re-deriving the alignment table there
 * would hand out a window PMP cannot express. */
#define PROTO_ELF_SLOT_SPAN_LOG2_MAX 4u
/* One flash-XIP arena slot. This is ABI geometry rather than a kernel detail:
 * the ABI-5 linker script, the manifest's declared span and the store's
 * executable ceiling all measure in these units. protelf_xip.h keeps its own
 * spelling and asserts the two agree. */
#define PROTO_ELF_XIP_SLOT_BYTES (128u * 1024u)
/* Large-heap requests are powers of two from 64 KiB through 8 MiB.
 * The boot-reserved 8 MiB pool is shared in independently protected slices. */
#define PROTO_ELF_LARGE_HEAP_LOG2_MIN 16u
#define PROTO_ELF_LARGE_HEAP_LOG2 23u
#define PROTO_ELF_LARGE_HEAP_BYTES (1u << PROTO_ELF_LARGE_HEAP_LOG2)
/* Largest executable any ABI-5 image may carry: the widest run the arena can
 * hand out. The exact per-image limit is its own declared span. */
#define PROTO_ELF_XIP_SPAN_BYTES_MAX \
    (PROTO_ELF_XIP_SLOT_BYTES << PROTO_ELF_SLOT_SPAN_LOG2_MAX)
#define PROTO_ELF_SIGNATURE_BYTES 72u
#define PROTO_ELF_MANIFEST_MAGIC 0x50454C46u  /* "PELF" */
#define PROTO_ELF_MANIFEST_VERSION_V1 1u
#define PROTO_ELF_MANIFEST_VERSION_V2 2u
#define PROTO_ELF_MANIFEST_VERSION_V3 3u
#define PROTO_ELF_MANIFEST_VERSION_V4 4u
#define PROTO_ELF_MANIFEST_VERSION 5u
/* V4 keeps the V3 struct layout. It advances the package generation marker
 * alongside ABI 13; V3 bundles remain loadable at their declared ABI floor.
 *
 * V5 keeps the same 420-byte struct but claims two previously-reserved
 * regions for the resource section: reserved[32] becomes the resource table
 * SHA-256, and the first 16 bytes of reserved_v2[24] become
 * {resource_count, table_size, payload_size, 0}. A V5 manifest with no
 * resources leaves both regions zero, so the fields are self-describing and
 * V4 bundles keep validating unchanged. */
#define PROTO_ELF_MANIFEST_VERSION_SUPPORTED(version_) \
    ((version_) == PROTO_ELF_MANIFEST_VERSION_V3 || \
     (version_) == PROTO_ELF_MANIFEST_VERSION_V4 || \
     (version_) == PROTO_ELF_MANIFEST_VERSION)
#define PROTO_ELF_MANIFEST_V1_SIZE 260u
#define PROTO_ELF_MANIFEST_V2_SIZE 356u
/* ABI 5 adds the flash-XIP layout and the resource API. ABI 4 images stay
 * loadable through the RAM path; only ABI 5 may set PROTO_ELF_LAYOUT_FLASH_XIP
 * or open resources. */
#define PROTO_ELF_ABI_VERSION 5u
#define PROTO_ELF_ABI_VERSION_V4 4u
#define PROTO_ELF_ABI_VERSION_SUPPORTED(abi_) \
    ((abi_) == PROTO_ELF_ABI_VERSION_V4 || (abi_) == PROTO_ELF_ABI_VERSION)
#define PROTO_ELF_LAYOUT_SPLIT_DYNAMIC (1u << 0)
/* Text is executed in place from the memory-mapped xip_apps partition; only
 * the writable segment is materialised in the paired PSRAM arena. Requires
 * ABI 5 and an app linked against the 0x08000000 flash/PSRAM delta. */
#define PROTO_ELF_LAYOUT_FLASH_XIP (1u << 1)
#define PROTO_ELF_LAYOUT_FLAGS_SUPPORTED \
    (PROTO_ELF_LAYOUT_SPLIT_DYNAMIC | PROTO_ELF_LAYOUT_FLASH_XIP)
/* Kernel-reported bit in ProtoElfLoadMap_t.flags (which otherwise carries the
 * manifest's app_flags): the running chip encodes SIMD as `xespv` (ESP32-P4
 * v3.x), not `xesppie` (v1.x). The kernel sets it from the silicon family it
 * was built for; a manifest can never set it, because the installer rejects
 * unknown app_flags bits. Apps built by protelf_dev.py carry both encodings of
 * their SIMD routines, and the SDK startup selects one from this bit before
 * proto_loaded_main(). Older kernels leave it clear, which reads as v1.x. */
#define PROTO_ELF_LOAD_MAP_FLAG_PIE_XESPV (1u << 31)
#define PROTO_ELF_LOAD_MAP_MAGIC 0x504C4D50u  /* "PLMP" */
#define PROTO_ELF_START_MAGIC 0x50535452u     /* "PSTR" */
#define PROTO_ELF_OBJECT_TABLE_MAGIC 0x504F424Au /* "POBJ" */
#define PROTO_ELF_OBJECT_TABLE_VERSION 1u
#define PROTO_ELF_MAX_OBJECTS 8u
#define PROTO_ELF_MAX_DEPENDENCIES 32u
#define PROTO_ELF_SONAME_MAX 48u
#define PROTO_ELF_OBJECT_FILE_MAX 48u

typedef uint32_t ProtoHandle_t;
typedef uint32_t ProtoBufferHandle_t;
typedef uint32_t ProtoDlHandle_t;
typedef uint32_t ProtoThreadHandle_t;

#define PROTO_INVALID_HANDLE 0u
#define PROTO_INVALID_BUFFER 0u
#define PROTO_INVALID_DL_HANDLE 0u
#define PROTO_INVALID_THREAD 0u

typedef enum {
    PROTO_OPEN_READ      = 1u << 0,
    PROTO_OPEN_WRITE     = 1u << 1,
    PROTO_OPEN_IOCTL     = 1u << 2,
    PROTO_OPEN_CREATE    = 1u << 3,
    PROTO_OPEN_TRUNCATE  = 1u << 4,
    PROTO_OPEN_APPEND    = 1u << 5,
    PROTO_OPEN_DIRECTORY = 1u << 6,
    PROTO_OPEN_EXCLUSIVE = 1u << 7
} ProtoOpenFlag_t;

/* INV-050: each of these flags changes the tree or a file's bytes at open,
 * so the storage backend accepts them only together with PROTO_OPEN_WRITE.
 * The single-writer census, the app reserve and the handle's write right all
 * key off WRITE; a mutation that skipped it would escape all three. */
#define PROTO_OPEN_MUTATING_FLAGS (PROTO_OPEN_CREATE | PROTO_OPEN_TRUNCATE | \
                                   PROTO_OPEN_APPEND | PROTO_OPEN_EXCLUSIVE)

typedef enum {
    PROTO_HANDLE_READ  = 1u << 0,
    PROTO_HANDLE_WRITE = 1u << 1,
    PROTO_HANDLE_IOCTL = 1u << 2,
    PROTO_HANDLE_CLOSE = 1u << 3,
    PROTO_HANDLE_SEND  = 1u << 4,
    PROTO_HANDLE_RECV  = 1u << 5,
    /* ABI 18 networking rights. Listener acceptance is deliberately distinct
     * from connected-socket data transfer, and half-close is not IOCTL. */
    PROTO_HANDLE_ACCEPT   = 1u << 6,
    PROTO_HANDLE_SHUTDOWN = 1u << 7
} ProtoHandleRight_t;

typedef enum {
    PROTO_BUFFER_READ  = 1u << 0,
    PROTO_BUFFER_WRITE = 1u << 1
} ProtoBufferFlag_t;

typedef enum {
    PROTO_PROCESS_ENTRY_BUILTIN = 1u,
    PROTO_PROCESS_ENTRY_LOADED_ELF = 2u
} ProtoProcessEntryKind_t;

typedef enum {
    PROTO_ELF_TRUST_NONE = 0u,
    PROTO_ELF_TRUST_DEV_UNSIGNED = 1u,
    PROTO_ELF_TRUST_RELEASE_SIGNED = 2u
} ProtoElfTrust_t;

typedef enum {
    PROTO_ELF_SIGNATURE_NONE = 0u,
    PROTO_ELF_SIGNATURE_ECDSA_P256_SHA256 = 1u
} ProtoElfSignaturePolicy_t;

typedef enum {
    PROTO_ELF_OBJECT_MAIN       = 1u << 0,
    PROTO_ELF_OBJECT_REQUIRED   = 1u << 1,
    PROTO_ELF_OBJECT_OPTIONAL   = 1u << 2,
    PROTO_ELF_OBJECT_SYSTEM     = 1u << 3,
    PROTO_ELF_OBJECT_SHARED_RX  = 1u << 4,
    PROTO_ELF_OBJECT_GLOBAL     = 1u << 5
} ProtoElfObjectFlag_t;

typedef enum {
    PROTO_DLOPEN_NOW    = 1u << 0,
    PROTO_DLOPEN_LOCAL  = 1u << 1,
    PROTO_DLOPEN_GLOBAL = 1u << 2
} ProtoDlOpenFlag_t;

typedef enum {
    PROTO_DL_RESULT_NEWLY_OPENED = 1u << 0,
    PROTO_DL_RESULT_LAST_CLOSE   = 1u << 1
} ProtoDlResultFlag_t;

typedef enum {
    PROTO_APP_PERMISSION_STORAGE_PRIVATE = 1u << 0,
    PROTO_APP_PERMISSION_IPC_LOGGER      = 1u << 1,
    PROTO_APP_PERMISSION_UI              = 1u << 2,
    PROTO_APP_PERMISSION_STORAGE_SHARED  = 1u << 3,
    PROTO_APP_PERMISSION_FILE_DIALOG     = 1u << 4,
    PROTO_APP_PERMISSION_WIFI            = 1u << 5,
    PROTO_APP_PERMISSION_AUDIO           = 1u << 6,
    /* Accept INBOUND TCP connections (ABI 18 / Phase 3). Deliberately separate
     * from PROTO_APP_PERMISSION_WIFI: outbound reach is a client relationship
     * the app initiates, while listening exposes a port on the user's network
     * to unauthenticated peers. WIFI must never imply this bit, and this bit is
     * useless without WIFI -- a listener still needs the station up, so the
     * kernel requires BOTH pairs (see wifi_authorize_listen). */
    PROTO_APP_PERMISSION_NETWORK_LISTEN  = 1u << 7,
    /* Direct use of the CPU-local PIE/SIMD register file. The kernel still
     * owns context activation, first-use clearing, and scheduling policy. */
    PROTO_APP_PERMISSION_SIMD            = 1u << 8
} ProtoAppPermission_t;

/* Every defined permission bit. Validators that reject "unknown" permission
 * bits MUST use this rather than re-listing the enum: four separate
 * hand-maintained copies of that list existed, and adding a permission without
 * updating all of them made sys_app_registry_selftest() fail, which panics the
 * kernel at boot with no hint as to which list was stale. Add a bit above and
 * it is included here automatically. */
#define PROTO_APP_PERMISSION_ALL \
    ((uint32_t)(PROTO_APP_PERMISSION_STORAGE_PRIVATE | \
                PROTO_APP_PERMISSION_IPC_LOGGER | \
                PROTO_APP_PERMISSION_UI | \
                PROTO_APP_PERMISSION_STORAGE_SHARED | \
                PROTO_APP_PERMISSION_FILE_DIALOG | \
                PROTO_APP_PERMISSION_WIFI | \
                PROTO_APP_PERMISSION_AUDIO | \
                PROTO_APP_PERMISSION_NETWORK_LISTEN | \
                PROTO_APP_PERMISSION_SIMD))

typedef enum {
    PROTO_APP_INSTALL_UPLOADED = 1u,
    PROTO_APP_INSTALL_VERIFIED = 2u,
    PROTO_APP_INSTALL_STAGED = 3u,
    PROTO_APP_INSTALL_PENDING_PERMISSIONS = 4u,
    PROTO_APP_INSTALL_INSTALLED = 5u
} ProtoAppInstallState_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t app_id;
    uint32_t preferred_pid;
    uint32_t abi_version;
    uint32_t min_kernel_abi;
    uint32_t security_profile;
    uint32_t required_caps;
    uint32_t grant_caps;
    uint32_t deny_caps;
    uint32_t storage_flags;
    uint32_t app_flags;
    uint32_t stack_depth;
    uint32_t priority;
    uint32_t core_id;
    uint32_t mailbox_depth;
    uint32_t memory_quota_bytes;
    uint32_t heap_initial_bytes;
    uint32_t heap_growth_step;
    uint32_t heap_region_limit;
    uint32_t memory_flags;
    uint32_t allowed_ipc_targets;
    uint32_t elf_size;
    uint32_t signature_policy;
    uint8_t  elf_sha256[PROTO_ELF_HASH_BYTES];
    char     cli_name[PROTO_ELF_NAME_MAX];
    char     display_name[PROTO_ELF_NAME_MAX];
    char     version_name[PROTO_ELF_NAME_MAX];
    uint8_t  reserved[32];
    char     package_name[PROTO_ELF_PACKAGE_MAX];
    uint32_t required_permissions;
    uint32_t optional_permissions;
    uint32_t granted_permissions;
    uint32_t denied_permissions;
    uint32_t install_state;
    /* Monotonic publisher release number. Unlike the human-readable
     * version_name, this value is covered by the release signature and is the
     * only version accepted by the kernel rollback ledger. */
    uint32_t release_version_code;
    uint8_t  reserved_v2[24];
    uint32_t object_count;
    uint32_t object_table_size;
    uint8_t  object_table_sha256[PROTO_ELF_HASH_BYTES];
    uint32_t dynamic_flags;
    /* Declared inbound listen port (Phase 3). Carved from reserved_v3, which
     * every existing manifest already zero-fills, so the BINARY LAYOUT AND
     * SIZE ARE UNCHANGED and no manifest version bump is required. Zero -- the
     * value every pre-Phase-3 bundle carries -- means "ephemeral only", which
     * is the fail-safe reading: an old manifest can never be interpreted as
     * declaring authority over a named port. */
    uint16_t listen_port;
    /* Requested flash-XIP arena span, as a power-of-two slot count exponent:
     * 0 => 1 slot (128 KiB RX + 128 KiB RW), 1 => 2 slots, 2 => 4 slots.
     * Carved from reserved_v3 the same way listen_port was, so the BINARY
     * LAYOUT AND SIZE ARE UNCHANGED. Zero -- the value every pre-span bundle
     * already carries -- means the single-slot pairing those images were
     * built and reviewed against, which is the fail-safe reading: an old
     * manifest can never widen its own isolation window. */
    uint8_t  slot_span_log2;
    /* Requested large heap, as a power-of-two byte count exponent. 0 => none;
     * nonzero values are PROTO_ELF_LARGE_HEAP_LOG2_MIN through
     * PROTO_ELF_LARGE_HEAP_LOG2. Carved from reserved_v3 like the two
     * fields above, so the BINARY LAYOUT AND SIZE ARE UNCHANGED. Zero -- the
     * value every existing bundle carries -- means no pool, which is the
     * fail-safe reading. The pool is memory for the app's own sandbox, not
     * authority over anything outside it, so it is a signed resource request
     * like slot_span_log2 rather than a capability. */
    uint8_t  large_heap_log2;
    uint8_t  reserved_v3[16];
} ProtoElfManifest_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t entry_size;
    uint32_t entry_count;
    uint32_t flags;
    uint8_t  reserved[8];
} ProtoElfObjectTableHeader_t;

typedef struct {
    uint32_t object_id;
    uint32_t flags;
    uint32_t abi_version;
    uint32_t trust;
    uint32_t file_size;
    uint32_t reserved0;
    uint8_t  sha256[PROTO_ELF_HASH_BYTES];
    char     soname[PROTO_ELF_SONAME_MAX];
    char     file_name[PROTO_ELF_OBJECT_FILE_MAX];
} ProtoElfObjectRecord_t;

typedef struct {
    uint32_t object_id;
    uint32_t flags;
    uint32_t trust;
    uint32_t hash32;
    uintptr_t load_bias;
    uint32_t  slot_offset;
    uint32_t  slot_size;
    uintptr_t rx_addr;
    size_t    rx_size;
    uintptr_t rw_addr;
    size_t    rw_size;
    uint32_t  elf_rx_vaddr;
    uint32_t  elf_rw_vaddr;
    uintptr_t dynamic_addr;
    uintptr_t got_addr;
    uintptr_t init_addr;
    uintptr_t fini_addr;
    uintptr_t init_array_addr;
    uint32_t  init_array_count;
    uintptr_t fini_array_addr;
    uint32_t  fini_array_count;
    char      soname[PROTO_ELF_SONAME_MAX];
} ProtoElfObjectMap_t;

typedef struct {
    uint8_t from_object;
    uint8_t to_object;
    uint8_t flags;
    uint8_t reserved;
} ProtoElfDependency_t;

typedef struct {
    uint32_t  magic;
    uint32_t  version;
    /* ABI v1 prefix. Keep these fields stable for existing loaded apps. */
    uintptr_t text_addr;
    size_t    text_size;
    uintptr_t data_addr;
    size_t    data_size;
    uintptr_t heap_addr;
    size_t    heap_size;
    uintptr_t entry_addr;
    uint32_t  flags;
    uint32_t  object_count;
    uint32_t  dependency_count;
    size_t    private_rx_bytes;
    size_t    shared_rx_bytes;
    size_t    private_rw_bytes;
    uint32_t  load_map_generation;
    uint32_t  init_count;
    uint32_t  fini_count;
    uint8_t   init_order[PROTO_ELF_MAX_OBJECTS];
    uint8_t   fini_order[PROTO_ELF_MAX_OBJECTS];
    ProtoElfObjectMap_t objects[PROTO_ELF_MAX_OBJECTS];
    ProtoElfDependency_t dependencies[PROTO_ELF_MAX_DEPENDENCIES];
} ProtoElfLoadMap_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t abi_version;
    uint32_t app_id;
    uint32_t pid;
    uint32_t generation;
    uint32_t trust;
    uint32_t image_size;
    uint32_t object_count;
    uint32_t load_map_generation;
    uint8_t  image_sha256[PROTO_ELF_HASH_BYTES];
} ProtoElfStartInfo_t;

/* ---------------------------------------------------------------------------
 * ABI 5 resource sections.
 *
 * Bulk assets ship beside the ELF instead of inside it, so a loaded app pays
 * flash rather than the RX window for its artwork and audio. The on-disk
 * layout is produced by tools/protelf_resources.py and independently
 * re-validated by store/server/protostore/resources.py; these structs are the
 * third parser and MUST stay byte-identical to both. Every size below is
 * asserted rather than assumed.
 *
 * Authentication is layered: each chunk carries the SHA-256 of its *stored*
 * (possibly compressed) bytes, so the kernel authenticates before it
 * decompresses and a hostile LZ4 stream never reaches the decoder. Each
 * resource carries the SHA-256 of its fully decoded content, and the whole
 * table's SHA-256 lives in the signed manifest.
 * -------------------------------------------------------------------------*/
#define PROTO_RESOURCE_TABLE_MAGIC 0x50525343u /* "PRSC" */
#define PROTO_RESOURCE_TABLE_VERSION 1u
#define PROTO_RESOURCE_NAME_MAX 48u
#define PROTO_RESOURCE_MAX_COUNT 64u
/* Chunks per resource table. The kernel keeps a running app's authenticated
 * table resident, so this bounds that copy at 32 + 64*96 + 4096*48 bytes
 * (~198 KiB): 15 MiB of uncompressed content, far past what a 4 MiB bundle
 * carries. The packer, Store and installer refuse larger tables. */
#define PROTO_RESOURCE_MAX_CHUNKS 4096u
/* Uncompressed bytes per chunk. */
#define PROTO_RESOURCE_CHUNK_BYTES 3840u
/* Stored bytes per chunk: CHUNK_BYTES plus worst-case LZ4 expansion headroom.
 * The packer additionally caps stored + uncompressed at 8192 so the device's
 * peak staging cost for one chunk stays bounded. */
#define PROTO_RESOURCE_STORED_MAX 3872u
#define PROTO_RESOURCE_CODEC_RAW 0u
#define PROTO_RESOURCE_CODEC_LZ4 1u

typedef uint32_t ProtoResourceHandle_t;
#define PROTO_INVALID_RESOURCE 0u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t resource_record_size;
    uint32_t resource_count;
    uint32_t chunk_record_size;
    uint32_t chunk_count;
    uint32_t payload_size;
} ProtoResourceTableHeader_t;

typedef struct {
    char     name[PROTO_RESOURCE_NAME_MAX];
    uint32_t uncompressed_size;
    uint32_t first_chunk;
    uint32_t chunk_count;
    uint32_t flags;
    uint8_t  sha256[PROTO_ELF_HASH_BYTES];
} ProtoResourceRecord_t;

typedef struct {
    uint32_t payload_offset;
    uint32_t stored_size;
    uint32_t uncompressed_size;
    uint32_t codec;
    uint8_t  sha256[PROTO_ELF_HASH_BYTES];
} ProtoResourceChunkRecord_t;

/* The packer emits "<8I", "<48s4I32s" and "<4I32s"; a padded struct here
 * would silently desynchronise the kernel from both Python parsers. */
_Static_assert(sizeof(ProtoResourceTableHeader_t) == 32u,
               "ProtoResourceTableHeader_t must match protelf_resources '<8I'");
_Static_assert(sizeof(ProtoResourceRecord_t) == 96u,
               "ProtoResourceRecord_t must match protelf_resources '<48s4I32s'");
_Static_assert(sizeof(ProtoResourceChunkRecord_t) == 48u,
               "ProtoResourceChunkRecord_t must match protelf_resources '<4I32s'");

typedef struct {
    char name[PROTO_RESOURCE_NAME_MAX];
} ProtoResourceOpenRequest_t;

typedef struct {
    uint32_t uncompressed_size;
    uint32_t chunk_count;
    uint32_t chunk_size;
    uint32_t flags;
} ProtoResourceInfo_t;

typedef struct {
    ProtoResourceHandle_t resource;
    ProtoResourceInfo_t info;
} ProtoResourceOpenResult_t;

typedef struct {
    ProtoResourceHandle_t resource;
    uint32_t chunk_index;
    ProtoBufferHandle_t buffer;
    uint32_t buffer_offset;
} ProtoResourceReadRequest_t;

typedef struct {
    uint32_t codec;
    uint32_t stored_size;
    uint32_t uncompressed_size;
    uint32_t resource_offset;
} ProtoResourceChunkInfo_t;

typedef struct {
    ProtoResourceHandle_t resource;
    uint32_t reserved;
} ProtoResourceCloseRequest_t;

typedef struct {
    ProtoDlHandle_t handle;
    uintptr_t address;
    uint32_t object_index;
    uint32_t flags;
} ProtoDlResult_t;

typedef struct {
    char soname[PROTO_ELF_SONAME_MAX];
    uint32_t flags;
} ProtoDlOpenRequest_t;

typedef struct {
    ProtoDlHandle_t handle;
    char symbol[PROTO_ELF_SONAME_MAX];
} ProtoDlSymRequest_t;

typedef struct {
    ProtoDlHandle_t handle;
} ProtoDlCloseRequest_t;

typedef struct {
    uint32_t object_id;
    uint32_t flags;
    uint32_t trust;
    uint32_t hash32;
    uint32_t image_bytes;
    uint32_t live_refs;
    char soname[PROTO_ELF_SONAME_MAX];
} ProtoElfLibraryInfo_t;

#define IPC_TARGET_NONE 0x00000000u
#define IPC_TARGET_ALL  0xFFFFFFFFu
#define IPC_TARGET(pid) ((uint32_t)(1u << (pid)))

typedef struct {
    uint32_t sender_pid;
    uint32_t msg_type;
    uint32_t timestamp_ms;
    size_t   payload_len;
    uint8_t  payload[IPC_PAYLOAD_SIZE];
} IpcMessage_t;

typedef enum {
    KERNEL_OK                      = 0,
    KERNEL_ERR_INVALID             = -1,
    KERNEL_ERR_NOT_FOUND           = -2,
    KERNEL_ERR_NO_MAILBOX          = -3,
    KERNEL_ERR_MAILBOX_FULL        = -4,
    KERNEL_ERR_TIMEOUT             = -5,
    KERNEL_ERR_DENIED              = -10,
    KERNEL_ERR_CAP_DENIED          = -11,
    KERNEL_ERR_LOG_DENIED          = -12,
    KERNEL_ERR_IPC_TARGET_DENIED   = -13,
    KERNEL_ERR_SECURITY_TERMINATED = -14,
    KERNEL_ERR_NOT_SUPPORTED       = -15,
    KERNEL_ERR_PERMISSION_REQUIRED = -16,
    KERNEL_ERR_DRIVER              = -20,
    /* The volume is full, or the write would eat into the kernel reserve that
     * keeps the OS bootable when apps fill storage. Mirrored in the
     * PROTO_ABI_TYPES_DEFINED fallback copy in kernel_types.h. */
    KERNEL_ERR_NO_SPACE            = -21,
    /* ABI 24. An exclusive create, a no-replace rename or a replace found the
     * name occupied (FAT case and short-name aliases count as occupied). */
    KERNEL_ERR_EXISTS              = -22,
    /* ABI 24. rmdir of a directory that still has entries. */
    KERNEL_ERR_NOT_EMPTY           = -23,
    /* ABI 24. The other end of a pipe, or the object a handle named, is gone. */
    KERNEL_ERR_CLOSED              = -24,
    /* ABI 24. The file is open through another handle and the operation
     * (resize, rename, replace, rmdir) cannot be made coherent for it. */
    KERNEL_ERR_BUSY                = -25
} KernelStatus_t;

#define PROTO_SHM_MAX_BYTES 4096u
#define PROTO_UI_MAX_COMMANDS 250u
#define PROTO_UI_TEXT_MAX 48u
#define PROTO_UI_ROOT_ID 0u
#define PROTO_UI_TEXT_DEFAULT 0u
#define PROTO_UI_TEXT_SMALL   10u
#define PROTO_UI_TEXT_MEDIUM  12u
#define PROTO_UI_TEXT_LARGE   18u
#define PROTO_GAME2D_MAX_ASSETS 64u
#define PROTO_GAME2D_MAX_ASSET_BYTES 131072u
#define PROTO_GAME2D_MAX_TOTAL_ASSET_BYTES 1048576u
#define PROTO_GAME2D_MAX_PALETTES 64u
#define PROTO_GAME2D_MAX_PALETTE_COLORS 256u
#define PROTO_GAME2D_MAX_DRAW_COMMANDS 256u
/*
 * The V2 ceiling, raised from 1024 to 2048 for richer arcade scenes.
 *
 * The kernel's reusable PSRAM submission snapshot is sized for this ceiling,
 * so the constant is both a validation bound and a fixed kernel allocation.
 * The per-frame cost of raising it is still only that
 * the compositor prepares command row bins and visits candidates per raster
 * band. More submitted commands add preparation/dispatch work; extra alpha
 * overdraw adds pixel work too. Measure both before filling this ceiling.
 * The two snapshot slots gain 64 KiB total at this bound. App-side command
 * capacity remains explicit, and the SDK default stays at 1024.
 *
 * This is a CEILING, not a default. Apps that pass 0 for command_capacity
 * still get PROTO_GFX_DEFAULT_COMMANDS, so raising it here does not silently
 * double any existing app's command buffer.
 */
#define PROTO_GAME2D_MAX_DRAW_COMMANDS_V2 2048u
#define PROTO_GAME2D_MAX_DIRTY_RECTS 16u
#define PROTO_GAME2D_TEXT_MAX 48u
#define PROTO_GAME2D_MAX_TEXT_BYTES 4096u
#define PROTO_GAME2D_MAX_CLIP_DEPTH 8u
#define PROTO_GAME2D_INVALID_ASSET 0u
#define PROTO_GAME2D_INVALID_PALETTE 0u
#define PROTO_TELEMETRY_HISTORY_DEPTH 24u
#define PROTO_PROCESS_SNAPSHOT_MAX 20u
#define PROTO_PERF_SNAPSHOT_MAX 16u
#define PROTO_NET_HOST_MAX 64u
#define PROTO_NET_SSID_MAX 32u
#define PROTO_UI_INPUT_MSG_TYPE 0x55490001u /* "UI" key event */
/* UI input's IpcMessage_t.timestamp_ms is captured at delivery, in monotonic
 * milliseconds (esp_timer clock, as used by proto_time_us), before queuing. */
#define PROTO_KEY_ESCAPE        0x1bu
#define PROTO_KEY_LEFT          0x80u
#define PROTO_KEY_RIGHT         0x81u
#define PROTO_KEY_UP            0x82u
#define PROTO_KEY_DOWN          0x83u
/* ABI 20 held-key snapshot (PROTO_SYS_KEY_STATE). The PROTO_UI_INPUT_MSG_TYPE
 * byte stream above has no key-up events; this does. Bit indices are USB HID
 * keyboard usage IDs (page 0x07) and modifiers also appear as usages
 * 0xE0..0xE7. Poll it once per frame. */
#define PROTO_KEY_STATE_VERSION   1u
#define PROTO_KEY_STATE_BYTES     32u
#define PROTO_KEY_STATE_CONNECTED 0x01u /* a USB keyboard is attached */
#define PROTO_KEY_STATE_FOCUSED   0x02u /* caller owns input; bits are live */
#define PROTO_KEYCODE_A           0x04u /* A..Z are 0x04..0x1D */
#define PROTO_KEYCODE_1           0x1Eu /* 1..9 are 0x1E..0x26 */
#define PROTO_KEYCODE_0           0x27u
#define PROTO_KEYCODE_ENTER       0x28u
#define PROTO_KEYCODE_ESCAPE      0x29u
#define PROTO_KEYCODE_BACKSPACE   0x2Au
#define PROTO_KEYCODE_TAB         0x2Bu
#define PROTO_KEYCODE_SPACE       0x2Cu
#define PROTO_KEYCODE_RIGHT       0x4Fu
#define PROTO_KEYCODE_LEFT        0x50u
#define PROTO_KEYCODE_DOWN        0x51u
#define PROTO_KEYCODE_UP          0x52u
#define PROTO_KEYCODE_LEFT_CTRL   0xE0u
#define PROTO_KEYCODE_LEFT_SHIFT  0xE1u
#define PROTO_KEYCODE_LEFT_ALT    0xE2u
#define PROTO_KEYCODE_RIGHT_CTRL  0xE4u
#define PROTO_KEYCODE_RIGHT_SHIFT 0xE5u
#define PROTO_KEYCODE_RIGHT_ALT   0xE6u
#define PROTO_KEYCODE_LETTER(c)   ((uint8_t)(PROTO_KEYCODE_A + ((c) - 'a')))
#define PROTO_KEY_BIT(bits, usage) \
    ((((bits)[(uint8_t)(usage) >> 3]) >> ((uint8_t)(usage) & 7u)) & 1u)

typedef struct {
    uint32_t version;   /* PROTO_KEY_STATE_VERSION */
    uint32_t sequence;  /* advances on every keyboard report */
    uint8_t  flags;     /* PROTO_KEY_STATE_CONNECTED | PROTO_KEY_STATE_FOCUSED */
    uint8_t  modifiers; /* HID boot modifier byte */
    uint16_t reserved;
    uint8_t  held[PROTO_KEY_STATE_BYTES];
    /* Keys that went down since this process's previous read, so a tap
     * shorter than one frame is never lost. Cleared by the read. */
    uint8_t  pressed[PROTO_KEY_STATE_BYTES];
} ProtoKeyState_t;

/* ABI 25 relative-pointer snapshot (PROTO_SYS_POINTER_STATE), fed by BLE HID
 * mice. Deltas accumulate between reads and are cleared by the caller's read,
 * so polling once per frame never loses motion; `pressed`/`released` latch
 * button edges the same way. Zeroed unless the caller owns the foreground
 * surface. Buttons: bit 0 left, 1 right, 2 middle, 3 back, 4 forward. */
#define PROTO_POINTER_STATE_VERSION   1u
#define PROTO_POINTER_STATE_CONNECTED 0x01u /* a mouse is attached */
#define PROTO_POINTER_STATE_FOCUSED   0x02u /* caller owns input; fields live */
#define PROTO_POINTER_BUTTON_LEFT     0x01u
#define PROTO_POINTER_BUTTON_RIGHT    0x02u
#define PROTO_POINTER_BUTTON_MIDDLE   0x04u

typedef struct {
    uint32_t version;   /* PROTO_POINTER_STATE_VERSION */
    uint32_t sequence;  /* advances on every mouse report */
    uint8_t  flags;     /* PROTO_POINTER_STATE_CONNECTED | _FOCUSED */
    uint8_t  buttons;   /* held now */
    uint8_t  pressed;   /* went down since this process's previous read */
    uint8_t  released;  /* went up since this process's previous read */
    int32_t  dx, dy;    /* counts since previous read; +x right, +y down */
    int16_t  wheel;     /* detents since previous read; + away from user */
    int16_t  hwheel;    /* + right */
    uint32_t reports;   /* mouse reports folded into this snapshot */
} ProtoPointerState_t;

/* ABI 26 gamepad snapshot (PROTO_SYS_GAMEPAD_STATE), fed by BLE HID game
 * controllers (Xbox Wireless Controller and standard HID gamepads). Sticks
 * and triggers are the current position, normalized by the kernel and with
 * no dead zone applied; `pressed`/`released` latch button edges between this
 * process's reads. Zeroed unless the caller owns the foreground surface. With
 * two pads attached, the one that reported last is shown. Button names follow
 * the Xbox layout: A bottom, B right, X left, Y top. */
#define PROTO_GAMEPAD_STATE_VERSION   1u
#define PROTO_GAMEPAD_STATE_CONNECTED 0x01u /* a gamepad is attached */
#define PROTO_GAMEPAD_STATE_FOCUSED   0x02u /* caller owns input; fields live */
#define PROTO_GAMEPAD_A          0x00000001u
#define PROTO_GAMEPAD_B          0x00000002u
#define PROTO_GAMEPAD_X          0x00000004u
#define PROTO_GAMEPAD_Y          0x00000008u
#define PROTO_GAMEPAD_LB         0x00000010u
#define PROTO_GAMEPAD_RB         0x00000020u
#define PROTO_GAMEPAD_VIEW       0x00000040u /* Back/Select */
#define PROTO_GAMEPAD_MENU       0x00000080u /* Start */
#define PROTO_GAMEPAD_LS         0x00000100u /* left stick click */
#define PROTO_GAMEPAD_RS         0x00000200u /* right stick click */
#define PROTO_GAMEPAD_GUIDE      0x00000400u /* Xbox/Home */
#define PROTO_GAMEPAD_SHARE      0x00000800u
#define PROTO_GAMEPAD_DPAD_UP    0x00001000u
#define PROTO_GAMEPAD_DPAD_DOWN  0x00002000u
#define PROTO_GAMEPAD_DPAD_LEFT  0x00004000u
#define PROTO_GAMEPAD_DPAD_RIGHT 0x00008000u
#define PROTO_GAMEPAD_AXIS_MAX   32767

typedef struct {
    uint32_t version;   /* PROTO_GAMEPAD_STATE_VERSION */
    uint32_t sequence;  /* advances on every gamepad report */
    uint8_t  flags;     /* PROTO_GAMEPAD_STATE_CONNECTED | _FOCUSED */
    uint8_t  pads;      /* gamepads attached */
    uint16_t reserved;
    uint32_t buttons;   /* PROTO_GAMEPAD_* held now */
    uint32_t pressed;   /* went down since this process's previous read */
    uint32_t released;  /* went up since this process's previous read */
    int16_t  lx, ly;    /* left stick, -32767..32767; +x right, +y down */
    int16_t  rx, ry;    /* right stick, same convention */
    uint16_t lt, rt;    /* triggers, 0..32767 */
    uint32_t reports;   /* gamepad reports folded into this snapshot */
} ProtoGamepadState_t;

/* ── ABI 22 theme ─────────────────────────────────────────────────────────
 * Colour roles, in the kernel's order (kernel/gui/theme/ui_theme.h asserts it).
 * Roles are appended, never renumbered; ProtoTheme_t reserves room for them. */
typedef enum {
    PROTO_THEME_ROLE_BG = 0,
    PROTO_THEME_ROLE_BG_GLOW,
    PROTO_THEME_ROLE_SURFACE,
    PROTO_THEME_ROLE_SURFACE_ALT,
    PROTO_THEME_ROLE_LINE,
    PROTO_THEME_ROLE_TEXT,
    PROTO_THEME_ROLE_TEXT_MUTED,
    PROTO_THEME_ROLE_TEXT_FAINT,
    PROTO_THEME_ROLE_ACCENT,
    PROTO_THEME_ROLE_ON_ACCENT,
    PROTO_THEME_ROLE_ACCENT_SOFT,
    PROTO_THEME_ROLE_DANGER,
    PROTO_THEME_ROLE_SUCCESS,
    PROTO_THEME_ROLE_WARNING,
    PROTO_THEME_ROLE_BRAND,
    PROTO_THEME_ROLE_BRAND_INK,
    /* Derived by the kernel, not authored: the accent mixed 30% into the
     * background. A selected row's fill; any text colour stays legible on it. */
    PROTO_THEME_ROLE_SELECTION,
    PROTO_THEME_ROLE_COUNT
} ProtoThemeRole_t;

#define PROTO_THEME_ROLE_SLOTS 24u   /* wire capacity; unused slots are 0 */
#define PROTO_THEME_VERSION    1u
#define PROTO_THEME_NAME_MAX   32u

/* A ProtoUiCommand_t colour may name a role instead of an RGB888 value: the
 * kernel substitutes the current theme's colour when the command is
 * submitted. The tag is exact (0x52 'R', two zero bytes, the role), so a
 * plain RGB888 colour, whose top byte is always zero, can never match it. */
#define PROTO_UI_ROLE_TAG      0x52000000u
#define PROTO_UI_ROLE(role)    (PROTO_UI_ROLE_TAG | ((uint32_t)(role) & 0xffu))
#define PROTO_UI_IS_ROLE(color) \
    (((color) & 0xffffff00u) == PROTO_UI_ROLE_TAG && \
     ((color) & 0xffu) < (uint32_t)PROTO_THEME_ROLE_COUNT)

typedef struct {
    uint32_t version;       /* PROTO_THEME_VERSION */
    uint32_t generation;    /* changes whenever any value below does */
    uint32_t rgb[PROTO_THEME_ROLE_SLOTS];      /* 0xRRGGBB by ProtoThemeRole_t */
    uint16_t rgb565[PROTO_THEME_ROLE_SLOTS];   /* the same, for Game2D */
    uint8_t  tile_radius_pct;  /* tile corner, % of the tile side */
    uint8_t  panel_radius;     /* logical px */
    uint8_t  control_radius;   /* logical px */
    uint8_t  badge_inverted;   /* 1: black block, orange 4 */
    char     name[PROTO_THEME_NAME_MAX];
} ProtoTheme_t;
#define PROTO_TETRIS_BENCH_CONFIG_MSG_TYPE 0x54423100u /* "TB1" config */
#define PROTO_TETRIS_BENCH_RESULT_MSG_TYPE 0x54423101u /* "TB1" result */
#define PROTO_TETRIS_BENCH_MODE_PLAY  0u
#define PROTO_TETRIS_BENCH_MODE_DENSE 1u
#define PROTO_GAME_BENCH_CONFIG_MSG_TYPE 0x47423100u /* "GB1" config */
#define PROTO_GAME_BENCH_VERSION 1u
#define PROTO_GAME_BENCH_ARM 1u
#define PROTO_GAME_BENCH_GO  2u

typedef struct {
    uint32_t version;
    uint32_t run_id;
    uint32_t state_seed;
    uint32_t frames;
    uint32_t operation;
} ProtoGameBenchConfig_t;

/* Typed control channel used only when the trusted terminal launches the
 * built-in kernel microbenchmark. Direct `run bench-test` executions retain
 * their ordinary BENCH log stream and do not enqueue terminal results. */
#define PROTO_KERNEL_BENCH_CONFIG_MSG_TYPE 0x4b423100u /* "KB1" config */
#define PROTO_KERNEL_BENCH_RESULT_MSG_TYPE 0x4b423101u /* "KB1" result */
#define PROTO_KERNEL_BENCH_DONE_MSG_TYPE   0x4b423102u /* "KB1" done */
#define PROTO_KERNEL_BENCH_VERSION 1u
#define PROTO_KERNEL_BENCH_NAME_MAX 32u
#define PROTO_REGTEST_RESULT_MSG_TYPE 0x52543101u /* "RT1" result */
/* PIE-PMP probe: the terminal sends one case (target address + which
 * instruction) to a freshly launched pie-pmp-probe process, which performs
 * exactly that one access. A forbidden access faults and the process is
 * contained; an allowed access logs a marker and exits cleanly. Multiple
 * forbidden accesses cannot share a process, so each case is its own launch. */
#define PROTO_PIE_PMP_CONFIG_MSG_TYPE 0x50503101u /* "PP1" config */
#define PROTO_PIE_PMP_VERSION 1u
typedef enum {
    PROTO_PIE_PMP_OP_VLOAD = 0u,   /* esp.vld.128 */
    PROTO_PIE_PMP_OP_VSTORE,       /* esp.vst.128 */
    PROTO_PIE_PMP_OP_QACC_STORE,   /* esp.st.qacc */
    PROTO_PIE_PMP_OP_SCALAR_LOAD,  /* lw twin */
    PROTO_PIE_PMP_OP_SCALAR_STORE  /* sw twin */
} ProtoPiePmpOp_t;
typedef struct {
    uint32_t version;
    uint32_t op;        /* ProtoPiePmpOp_t */
    uint32_t target;    /* absolute address to touch */
    uint32_t expect_allowed; /* 1 = access should succeed */
} ProtoPiePmpConfig_t;
/* Network stack plan Phase 2 victim (docs/network-stack-plan.md). The
 * terminal launches net-stack-victim[-m], sends one config naming where to
 * block, waits for the BLOCKED report, then kills it and times teardown. A
 * killed process cannot be reused, so each case is its own launch. */
#define PROTO_NETSTACK_VICTIM_CONFIG_MSG_TYPE 0x4e533201u /* "NS2" config */
#define PROTO_NETSTACK_VICTIM_REPORT_MSG_TYPE 0x4e533202u /* "NS2" report */
#define PROTO_NETSTACK_VICTIM_VERSION 1u
typedef enum {
    PROTO_NETSTACK_VICTIM_IDLE = 1u, /* control: 500 ms sleeps, no socket */
    PROTO_NETSTACK_VICTIM_READ,      /* idle read on a loopback pair */
    PROTO_NETSTACK_VICTIM_WRITE,     /* write into a full window */
    PROTO_NETSTACK_VICTIM_ACCEPT,    /* accept with nothing pending */
    PROTO_NETSTACK_VICTIM_POLL,      /* poll an idle socket for READABLE */
    PROTO_NETSTACK_VICTIM_SERVE,     /* listen, accept one peer, read it */
    PROTO_NETSTACK_VICTIM_CLIENT,    /* connect to `port`, read until close */
    PROTO_NETSTACK_VICTIM_CONNECT,   /* Phase 3: handshake to an unroutable host */
    PROTO_NETSTACK_VICTIM_DNS,       /* Phase 3: lookup held by the stall hook */
    PROTO_NETSTACK_VICTIM_POLL_MANY, /* Phase 4: two idle sockets in one POLL */
    /* Phase 4 reactor, cross-process wake: TIMED_SERVE accepts one peer and
     * parks in a read; TIMED_SEND connects to `port`, waits, then sends its
     * send time (proto_time_us, one system-wide clock). The server reports
     * DATA with bytes = microseconds from that send to its read returning. */
    PROTO_NETSTACK_VICTIM_TIMED_SERVE,
    PROTO_NETSTACK_VICTIM_TIMED_SEND
} ProtoNetStackVictimMode_t;
/* Phase 3 hosts. TEST-NET-1 (RFC 5737) is never routed, so a SYN to it goes
 * unanswered. Both names are under `.invalid` (RFC 6761), which never
 * resolves: the stall name is held pending by the kernel's regtest hook,
 * the NX name gets a real NXDOMAIN (or a DNS timeout) from the network. */
#define PROTO_NETSTACK_UNROUTABLE_HOST "192.0.2.1"
#define PROTO_NETSTACK_STALL_HOST      "netstack-stall.invalid"
#define PROTO_NETSTACK_NXDOMAIN_HOST   "netstack-nx.invalid"
typedef enum {
    PROTO_NETSTACK_EVENT_BLOCKED = 1u,  /* entering its forever-wait */
    PROTO_NETSTACK_EVENT_LISTENING,     /* SERVE: `port` is bound */
    PROTO_NETSTACK_EVENT_PEER_CLOSED,   /* CLIENT: read ended, see rc/bytes */
    PROTO_NETSTACK_EVENT_FAILED,        /* setup or an unexpected return */
    PROTO_NETSTACK_EVENT_DATA           /* TIMED_SERVE: bytes = wake latency (us) */
} ProtoNetStackVictimEvent_t;
typedef struct {
    uint32_t version;
    uint32_t mode;      /* ProtoNetStackVictimMode_t */
    uint16_t port;      /* CLIENT only */
    uint16_t reserved;
} ProtoNetStackVictimConfig_t;
typedef struct {
    uint32_t version;
    uint32_t mode;
    uint32_t event;     /* ProtoNetStackVictimEvent_t */
    int32_t  rc;
    uint32_t bytes;
    uint16_t port;
    uint16_t reserved;
} ProtoNetStackVictimReport_t;
#define PROTO_REGTEST_RESULT_VERSION 1u

typedef struct {
    uint32_t version;
    uint32_t capture_id;
} ProtoKernelBenchConfig_t;

typedef struct {
    uint32_t version;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
} ProtoRegtestResult_t;

typedef struct {
    uint32_t version;
    uint32_t capture_id;
    uint32_t case_index;
    uint32_t duration_us;
    uint32_t operations;
    uint32_t average_us;
    uint32_t ok;
    char     name[PROTO_KERNEL_BENCH_NAME_MAX];
} ProtoKernelBenchResult_t;

typedef struct {
    uint32_t version;
    uint32_t capture_id;
    uint32_t case_count;
    uint32_t case_failures;
    uint32_t report_failures;
} ProtoKernelBenchDone_t;

typedef struct {
    uint32_t run_id;
    uint32_t piece_seed;
    uint32_t input_seed;
    uint32_t frames;
    uint32_t mode;
    uint32_t variant;
} ProtoTetrisBenchConfig_t;

typedef struct {
    uint32_t run_id;
    uint32_t piece_seed;
    uint32_t input_seed;
    uint32_t frames;
    uint32_t mode;
    uint32_t duration_us;
    uint32_t fps_x100;
    uint32_t submit_avg_us;
    uint32_t submit_max_us;
    uint32_t avg_commands;
    uint32_t max_commands;
    uint32_t step_avg_us;
    uint32_t step_max_us;
    uint32_t submit_failures;
} ProtoTetrisBenchResult_t;

typedef enum {
    PROTO_SYS_WRITE_LOG = 0,
    PROTO_SYS_MALLOC,
    PROTO_SYS_FREE,
    PROTO_SYS_SEND,
    PROTO_SYS_RECV,
    PROTO_SYS_WAIT_READY,
    PROTO_SYS_SLEEP,
    PROTO_SYS_EXIT,
    PROTO_SYS_DEPRECATED_GPIO,
    PROTO_SYS_POLL_RESPONSE,

    // Append-only ABI extension slots. These IDs were reserved in ABI v1 and
    // are now implemented incrementally; their numeric values remain frozen
    // so older user SDKs keep the same wire contract.
    PROTO_SYS_OPEN,
    PROTO_SYS_READ,
    PROTO_SYS_WRITE,
    PROTO_SYS_IOCTL,
    PROTO_SYS_CLOSE,
    PROTO_SYS_STAT,
    PROTO_SYS_MKDIR,
    PROTO_SYS_UNLINK,
    PROTO_SYS_GRAPHICS,
    PROTO_SYS_INPUT,
    PROTO_SYS_BUFFER_REGISTER,
    PROTO_SYS_BUFFER_UNREGISTER,
    PROTO_SYS_READ_BUF,
    PROTO_SYS_WRITE_BUF,
    PROTO_SYS_TIME_US,
    PROTO_SYS_DLOPEN,
    PROTO_SYS_DLSYM,
    PROTO_SYS_DLCLOSE,
    PROTO_SYS_ALLOCATOR_SNAPSHOT,
    PROTO_SYS_FILE_DIALOG,
    PROTO_SYS_NET,
    PROTO_SYS_KERNEL_INFO,
    PROTO_SYS_TRUSTED_PROMPT,
    /* Append-only lifecycle request; existing syscall IDs remain stable. */
    PROTO_SYS_UI_EXIT_REQUEST,
    /* Per-process worker threads. U-mode callers receive an independent
     * sealed stack/PMP context; SYSTEM M-mode callers use native workers. */
    PROTO_SYS_THREAD_CREATE,
    /* Append-only worker lifecycle operations. JOIN is deliberately a
     * nonblocking kernel primitive; the SDK implements bounded waiting
     * without ever parking the global syscall dispatcher. */
    PROTO_SYS_THREAD_JOIN,
    PROTO_SYS_THREAD_DETACH,
    /* Append-only positioned file I/O. Each operation carries its own byte
     * offset and never disturbs the handle's sequential position, so two
     * workers sharing one file handle cannot interleave a seek with the
     * transfer it was meant to position. Regular files only: streams and
     * devices have no meaningful offset and reject both operations. */
    PROTO_SYS_PREAD,
    PROTO_SYS_PWRITE,
    /* Append-only ABI 5 resource streaming. Reads land one authenticated
     * chunk at a time into a registered buffer, so an app can consume an
     * asset far larger than its heap without the kernel ever holding the
     * whole thing. Loaded ELF processes holding KERNEL_CAP_RESOURCE only. */
    PROTO_SYS_RESOURCE_OPEN,
    PROTO_SYS_RESOURCE_READ,
    PROTO_SYS_RESOURCE_CLOSE,
    /* Absolute, wrap-safe deadline on the same 32-bit monotonic clock
     * returned by PROTO_SYS_TIME_US. Unlike PROTO_SYS_SLEEP, this lets frame
     * pacing advance one stable timeline instead of accumulating work-time
     * and millisecond-rounding drift. */
    PROTO_SYS_SLEEP_UNTIL_US,
    /* ABI 19. A pure scheduler yield: the caller reaches a trap boundary,
     * gives the core to the next runnable task at its priority, and resumes.
     * PROTO_SYS_SLEEP with a 1 ms duration was the only yield an app had, and
     * at the idle priority every U-mode worker runs at (see scheduler.c) that
     * is not a one-millisecond pause -- it is a round-robin wait behind both
     * idle tasks and every other app worker, measured at several milliseconds.
     * A cooperative loop that yields between units of work paid that on every
     * unit. This carries no payload, changes no process state, and grants no
     * authority; it is gated on KERNEL_CAP_SLEEP purely because it is the same
     * class of operation as sleeping. */
    PROTO_SYS_YIELD,
    PROTO_SYS_STORE,
    /* PROTO_SYS_STORE is the ABI 19 append-only Store trust broker. The
     * built-in Store is the only entitled caller; bulk inputs and canonical
     * outputs use registered buffers, while install sources are caller-owned
     * read-only file handles. Existing syscall IDs never change. */
    PROTO_SYS_KEY_STATE,
    /* PROTO_SYS_KEY_STATE is the ABI 20 held-key snapshot from the USB HID
     * keyboard broker. It writes a ProtoKeyState_t through a validated user
     * pointer; state is zeroed unless the caller owns the foreground UI
     * surface and no trusted prompt or file dialog holds input. */
    /* ABI 21: the built-in U-mode Weather app reads the shared location and
     * publishes resolved coordinates through a narrow kernel broker. */
    PROTO_SYS_TIME_LOCATION,
    /* ABI 22: read the system theme (ProtoTheme_t). Public and read-only: the
     * colours are not sensitive, so any process may ask, with no capability.
     * Writing the theme stays kernel-side (Settings, admin session). */
    PROTO_SYS_THEME,
    /* ABI 23: the console shell's theme sounds (docs/sound-syscall-plan.md).
     * Entitled to the built-in shell only; every other caller is DENIED. Apps
     * that want sound have /dev/audio. Register-sized scalars, no pointers. */
    PROTO_SYS_SOUND,
    /* ABI 24 file lifecycle and feature discovery. Other operation IDs are
     * reserved; callers must check discovery before using optional features. */
    PROTO_SYS_FOUNDATION,
    /* ABI 25: relative-pointer snapshot (ProtoPointerState_t) from the BLE HID
     * mouse broker. Same gate and shape as PROTO_SYS_KEY_STATE. */
    PROTO_SYS_POINTER_STATE,
    /* ABI 26: gamepad snapshot (ProtoGamepadState_t) from the BLE HID
     * gamepad broker. Same gate and shape as PROTO_SYS_POINTER_STATE. */
    PROTO_SYS_GAMEPAD_STATE,
} ProtoSyscallId_t;

#define PROTO_THREAD_NAME_MAX 16u
#define PROTO_THREAD_MIN_STACK_BYTES 2048u
#define PROTO_THREAD_MAX_STACK_BYTES 16384u
#define PROTO_THREAD_MAX_PER_PROCESS 4u
#define PROTO_THREAD_WAIT_FOREVER UINT32_MAX
/* Slot zero is the process entry thread; the remaining slots are U workers. */
#define PROTO_UMODE_THREAD_MAX_PER_PROCESS (PROTO_THREAD_MAX_PER_PROCESS + 1u)

typedef enum {
    /* Let ESP-IDF's supported dual-core scheduler select either CPU. */
    PROTO_THREAD_CORE_ANY = 0u,
    PROTO_THREAD_CORE_0 = 1u,
    PROTO_THREAD_CORE_1 = 2u,
} ProtoThreadCorePolicy_t;

/* Opt-in budgeted realtime class for one latency-critical worker per
 * process, such as an audio producer feeding its 64 ms stream ring. Pass it
 * as ProtoThreadConfig_t.priority (0..2 remain plain hints that never change
 * scheduling). The worker runs above the display tasks but may use only a
 * bounded share of one core per period; overrunning demotes it to idle
 * priority until the next period. Requires the audio permission. At most one
 * per process and two system-wide. Kernels before this class reject the
 * value with KERNEL_ERR_INVALID, so create at a plain priority on failure. */
#define PROTO_THREAD_PRIORITY_REALTIME_AUDIO 0x100u

/* A by-value creation request. The kernel never accepts task handles, queues,
 * registry pointers, or a caller-selected owner. The owner is always the
 * authenticated calling process. */
typedef struct {
    uintptr_t entry;
    uintptr_t argument;
    uint32_t stack_bytes; /* Requested private user stack for U-mode workers. */
    uint32_t priority;
    uint32_t core_policy;
    char name[PROTO_THREAD_NAME_MAX];
} ProtoThreadConfig_t;

/* Shared JOIN/DETACH wire payload. reserved must be zero so a future ABI can
 * assign it semantics while preserving this request's wire shape. */
typedef struct {
    ProtoThreadHandle_t thread;
    uint32_t reserved;
} ProtoThreadLifecycleRequest_t;

// Kernel-internal code still uses the older SysCallType_t name. Keep it as an
// ABI alias so the wire format is one table, not two drifting enums.
typedef ProtoSyscallId_t SysCallType_t;

#define SYS_CALL_PRINT         PROTO_SYS_WRITE_LOG
#define SYS_CALL_ALLOCATE      PROTO_SYS_MALLOC
#define SYS_CALL_FREE          PROTO_SYS_FREE
#define SYS_CALL_SEND          PROTO_SYS_SEND
#define SYS_CALL_RECV          PROTO_SYS_RECV
#define SYS_CALL_WAIT_READY    PROTO_SYS_WAIT_READY
#define SYS_CALL_SLEEP         PROTO_SYS_SLEEP
#define SYS_CALL_EXIT          PROTO_SYS_EXIT
#define SYS_CALL_POLL_RESPONSE PROTO_SYS_POLL_RESPONSE

typedef enum {
    KERNEL_GPIO_OP_CONFIGURE = 0,
    KERNEL_GPIO_OP_WRITE,
    KERNEL_GPIO_OP_READ
} KernelGpioOp_t;

typedef enum {
    KERNEL_GPIO_MODE_DISABLE = 0,
    KERNEL_GPIO_MODE_INPUT,
    KERNEL_GPIO_MODE_OUTPUT,
    KERNEL_GPIO_MODE_INPUT_OUTPUT
} KernelGpioMode_t;

typedef enum {
    KERNEL_GPIO_PULL_NONE = 0,
    KERNEL_GPIO_PULL_UP,
    KERNEL_GPIO_PULL_DOWN,
    KERNEL_GPIO_PULL_UP_DOWN
} KernelGpioPull_t;

typedef struct {
    KernelGpioOp_t   op;
    int              pin;
    KernelGpioMode_t mode;
    KernelGpioPull_t pull;
    uint32_t         level;
} KernelGpioRequest_t;

typedef struct {
    int      return_code;
    uint32_t level;
} KernelGpioResult_t;

typedef enum {
    PROTO_IOCTL_GPIO_CONFIGURE = 0,
    PROTO_IOCTL_SHM_MAP,
    PROTO_IOCTL_SHM_STAT,
    PROTO_IOCTL_FILE_REWIND_TRUNCATE,
    PROTO_IOCTL_AUDIO_CONFIGURE,
    PROTO_IOCTL_AUDIO_VOLUME,
    PROTO_IOCTL_AUDIO_TONE,
    PROTO_IOCTL_AUDIO_FLUSH,
    PROTO_IOCTL_AUDIO_STAT
} ProtoIoctlOp_t;

/* ------------------------------------------------------------------
 * /dev/audio — kernel-owned mixer output
 * ------------------------------------------------------------------
 * A handle on /dev/audio is one mixer stream, not the codec. The kernel owns
 * the ES8311/I2S hardware for the life of the system; apps only ever push PCM
 * into a per-stream ring that the kernel mixes. There is deliberately no way
 * to reach the codec registers, the PA enable pin, or another stream's ring
 * from user space.
 *
 * PCM is signed 16-bit little-endian, and is resampled by the kernel from the
 * stream's declared rate to the device rate. Declaring a rate the device does
 * not natively run is normal and supported; it costs a little quality, never
 * an error. */
#define PROTO_AUDIO_MIN_SAMPLE_RATE 4000u
#define PROTO_AUDIO_MAX_SAMPLE_RATE 48000u
#define PROTO_AUDIO_MAX_CHANNELS    2u
/* Ceiling on a single tone request so one ioctl cannot occupy a mixer slot
 * indefinitely; longer sounds go through the PCM write path where the ring
 * depth bounds them naturally. */
#define PROTO_AUDIO_MAX_TONE_MS     5000u

typedef struct {
    uint32_t sample_rate;   /* Hz, PROTO_AUDIO_MIN/MAX_SAMPLE_RATE inclusive */
    uint32_t channels;      /* 1 = mono, 2 = interleaved stereo */
    uint32_t bits;          /* 16 only; other widths are rejected, not coerced */
    uint32_t volume_pct;    /* initial per-stream gain, 0..100 */
} ProtoAudioConfig_t;

typedef struct {
    uint32_t queued_bytes;    /* PCM accepted but not yet mixed */
    uint32_t capacity_bytes;  /* ring size, so callers can pace writes */
    uint32_t underruns;       /* ring ran dry while the stream was active */
    uint32_t overruns;        /* writes refused because the ring was full */
    uint32_t sample_rate;     /* device rate, after any resampling */
    uint32_t active_streams;  /* streams currently mixed, this one included */
} ProtoAudioStat_t;

typedef enum {
    PROTO_FILE_DIALOG_BEGIN = 1u,
    PROTO_FILE_DIALOG_POLL,
    PROTO_FILE_DIALOG_CANCEL
} ProtoFileDialogOp_t;

typedef enum {
    PROTO_FILE_DIALOG_OPEN = 1u,
    PROTO_FILE_DIALOG_SAVE_AS
} ProtoFileDialogType_t;

typedef enum {
    PROTO_FILE_DIALOG_PENDING = 1u,
    PROTO_FILE_DIALOG_APPROVED,
    PROTO_FILE_DIALOG_CANCELLED
} ProtoFileDialogStatus_t;

typedef struct {
    uint32_t op;
    uint32_t token;
    uint32_t type;
    uint32_t requested_rights;
    char suggested_name[PROTO_NAME_MAX];
    char suffix_filter[12];
} ProtoFileDialogRequest_t;

typedef struct {
    uint32_t status;
    uint32_t token;
    ProtoHandle_t handle;
    char path[PROTO_PATH_MAX];
} ProtoFileDialogResult_t;

#define PROTO_TRUSTED_REASON_MAX 64u

typedef enum {
    PROTO_TRUSTED_BEGIN = 1u,
    PROTO_TRUSTED_POLL,
    PROTO_TRUSTED_CANCEL
} ProtoTrustedPromptOp_t;

typedef enum {
    PROTO_TRUSTED_USER_VERIFY = 1u,
    PROTO_TRUSTED_PERMISSION,
    PROTO_TRUSTED_SYSTEM_ACTION,
    /* A foreground app-declared background transition. This is a confirmation
     * only prompt; it never grants a permission or executes an action. If the
     * user declines, cancels, or times out, the kernel continues through the
     * bounded exit-hook/force-reap path. */
    PROTO_TRUSTED_BACKGROUND_EXIT,
    /* SYSTEM-only foreground close confirmation; approval always terminates. */
    PROTO_TRUSTED_FORCE_EXIT
} ProtoTrustedPromptKind_t;

/* Generic launcher-to-live-app control channel.  The kernel is the only
 * sender; apps may consume commands only from their own mailbox. */
#define PROTO_APP_CONTROL_MSG_TYPE 0x41504301u /* "APC1" */
#define PROTO_APP_CONTROL_SHOW_UI  1u
#define PROTO_APP_CONTROL_EXIT_REQUEST 2u
#define PROTO_APP_CONTROL_BACKGROUND_ENTER 3u

typedef enum {
    PROTO_EXIT_REASON_USER_ESCAPE = 1u,
    PROTO_EXIT_REASON_USER_CLOSE,
    PROTO_EXIT_REASON_SYSTEM_FORCE,
} ProtoExitReason_t;

typedef enum {
    PROTO_EXIT_REQUEST_GRACEFUL = 1u << 0,
    PROTO_EXIT_REQUEST_FORCE    = 1u << 1,
    PROTO_EXIT_REQUEST_SKIP_BACKGROUND = 1u << 2,
} ProtoExitRequestFlag_t;

typedef struct {
    uint32_t version;
    uint32_t sequence;
    uint32_t reason;
    uint32_t deadline_ms;
    uint32_t flags;
} ProtoAppExitRequest_t;

typedef struct {
    uint32_t reason;
    uint32_t flags;
} ProtoUiExitRequest_t;

typedef struct {
    void  *buffer; /* ProtoKeyState_t */
    size_t len;
} ProtoKeyStateRequest_t;

typedef struct {
    void  *buffer; /* ProtoPointerState_t */
    size_t len;
} ProtoPointerStateRequest_t;

typedef struct {
    void  *buffer; /* ProtoGamepadState_t */
    size_t len;
} ProtoGamepadStateRequest_t;

typedef struct {
    void  *buffer; /* ProtoTheme_t */
    size_t len;
} ProtoThemeRequest_t;

/* ABI 23 sound. Ids are stable and append-only. Event ids match Pocket's
 * PkSfx_t so the shell passes them straight through. */
typedef enum {
    PROTO_SOUND_OP_EVENT = 1u,   /* play `event` in `theme` (a preview may name
                                    a theme other than the current one) */
    PROTO_SOUND_OP_MUSIC = 2u,   /* Home music in `theme`, or OFF */
} ProtoSoundOp_t;

typedef enum {
    PROTO_SOUND_THEME_BUBBLEGUM = 0u,
    PROTO_SOUND_THEME_CANDY = 1u,
    PROTO_SOUND_THEME_SQUEAKY = 2u,
    PROTO_SOUND_THEME_COUNT,
    PROTO_SOUND_THEME_OFF = 0xFFu,   /* MUSIC only */
} ProtoSoundTheme_t;

typedef enum {
    PROTO_SOUND_MOVE = 1u,       /* arg: pitch step 0..4, 4 = highest (top/right) */
    PROTO_SOUND_SELECT,
    PROTO_SOUND_BACK,
    PROTO_SOUND_TAB,             /* arg: 1 forward/right, 0 back/left */
    PROTO_SOUND_BUMP,
    PROTO_SOUND_LAUNCH,
    PROTO_SOUND_NOTIFY,
    PROTO_SOUND_SEND,
    PROTO_SOUND_ATTACH,
    PROTO_SOUND_SUCCESS,
    PROTO_SOUND_PARTY,
    PROTO_SOUND_ERROR,
    PROTO_SOUND_BOOT,
    PROTO_SOUND_TYPE,            /* arg: pitch step 0..4 */
    /* The first boot (docs/first-boot-sound.md). The kernel plays the startup
     * and the rhythm stage's song from its own data; the shell reports what
     * happened. These take arg 0..PROTO_SOUND_FB_ARG_MAX and ignore the theme. */
    PROTO_SOUND_FB_BEGIN,        /* start the startup's sounds */
    PROTO_SOUND_FB_POWER,        /* the power-on press; the song starts 1.6 s later. arg bit 0: the finale has the drop */
    PROTO_SOUND_FB_STATE,        /* arg: band players 0..3 | fever << 2 | two in a row << 3 */
    PROTO_SOUND_FB_YOU,          /* a clean hit; arg: bar | kind << 5 (0 four, 1 and, 2 held four) */
    PROTO_SOUND_FB_YOU_OK,       /* the same, slightly off the beat */
    PROTO_SOUND_FB_STRAY,        /* a press with no beat near it */
    PROTO_SOUND_FB_MISS,         /* a beat went by */
    PROTO_SOUND_FB_ASSIST,       /* arg: bar; the band covers that bar's four */
    PROTO_SOUND_FB_DOT,          /* arg: 1 the player let go, 0 the band plays it */
    PROTO_SOUND_FB_STOP,         /* leave the first boot */
    PROTO_SOUND_EVENT_COUNT,
} ProtoSoundEvent_t;

#define PROTO_SOUND_ARG_MAX 4u
#define PROTO_SOUND_FB_ARG_MAX 127u

typedef struct {
    uint8_t op;      /* ProtoSoundOp_t */
    uint8_t theme;   /* ProtoSoundTheme_t */
    uint8_t event;   /* ProtoSoundEvent_t, EVENT only */
    uint8_t arg;     /* 0..PROTO_SOUND_ARG_MAX (first boot: ..._FB_ARG_MAX), EVENT only */
} ProtoSoundRequest_t;

typedef enum {
    PROTO_TRUSTED_PENDING = 1u,
    PROTO_TRUSTED_APPROVED,
    PROTO_TRUSTED_DENIED,
    PROTO_TRUSTED_CANCELLED,
    PROTO_TRUSTED_FAILED
} ProtoTrustedPromptStatus_t;

typedef enum {
    PROTO_AUTH_PURPOSE_GENERIC = 1u,
    PROTO_AUTH_PURPOSE_UNLOCK_SECRET,
    PROTO_AUTH_PURPOSE_REVEAL_SECRET,
    PROTO_AUTH_PURPOSE_EXPORT_DATA,
    PROTO_AUTH_PURPOSE_CLEAR_APP_DATA,
    PROTO_AUTH_PURPOSE_UNINSTALL_APP,
    PROTO_AUTH_PURPOSE_CHANGE_SECURITY
} ProtoAuthPurpose_t;

typedef enum {
    PROTO_PERMISSION_GRANT_ONCE = 1u,
    PROTO_PERMISSION_GRANT_ALWAYS
} ProtoPermissionGrant_t;

typedef enum {
    PROTO_SYSTEM_ACTION_WIFI_ENABLE = 1u,
    PROTO_SYSTEM_ACTION_WIFI_DISABLE,
    PROTO_SYSTEM_ACTION_WIFI_DISCONNECT
} ProtoSystemAction_t;

typedef struct {
    uint32_t op;
    uint32_t token;
    uint32_t kind;
    uint32_t subject;
    uint32_t flags;
    char reason[PROTO_TRUSTED_REASON_MAX];
} ProtoTrustedPromptRequest_t;

typedef struct {
    uint32_t status;
    uint32_t token;
    int32_t  result_code;
    uint32_t granted_permissions;
} ProtoTrustedPromptResult_t;

/* Kernel Store broker. These types are public because the built-in Store runs
 * in U-mode, but possession of their layout grants no authority: the kernel
 * additionally authenticates the exact built-in identity, PCB generation,
 * foreground owner, launch admin session, and private entitlement. */
#define PROTO_STORE_BROKER_NONCE_BYTES 12u
#define PROTO_STORE_BROKER_NONCE_HEX_BYTES \
    (PROTO_STORE_BROKER_NONCE_BYTES * 2u + 1u)

typedef enum {
    PROTO_STORE_CREATE_QUERY = 1u,
    PROTO_STORE_VALIDATE_CATALOG,
    PROTO_STORE_VALIDATE_DETAIL,
    PROTO_STORE_VALIDATE_SCREENSHOT,
    PROTO_STORE_INSTALL_BEGIN,
    PROTO_STORE_INSTALL_POLL,
    PROTO_STORE_INSTALL_CANCEL,
    /*
     * Read-only projection of the kernel's installed-version ledger.
     *
     * Appended, so every existing op keeps its number. This does not touch
     * the loaded-app ABI: broker_entitled() admits only the built-in Store
     * by app_id and BUILTIN entry kind, so the interface lives entirely
     * inside one firmware image and cannot skew against a loaded binary.
     */
    PROTO_STORE_LEDGER_LIST
} ProtoStoreBrokerOp_t;

typedef enum {
    PROTO_STORE_JOB_PENDING = 1u,
    PROTO_STORE_JOB_VALIDATING,
    PROTO_STORE_JOB_AWAITING_PASSWORD,
    PROTO_STORE_JOB_INSTALLING,
    PROTO_STORE_JOB_COMPLETE,
    PROTO_STORE_JOB_CANCELLED,
    PROTO_STORE_JOB_FAILED
} ProtoStoreBrokerJobStatus_t;

#define PROTO_STORE_INSTALL_REPLACE (1u << 0)
/* Delta update (INV-052). With REPLACE only: bundle_handle holds the bundle
 * up to, not including, its resource table. The broker appends the installed
 * package's resource copies and admits the result only if the whole rebuilt
 * bundle hashes to the signed catalog bundle_sha256. A mismatch fails before
 * any prompt; the Store then retries with the full bundle. */
#define PROTO_STORE_INSTALL_REUSE_RESOURCES (1u << 1)

typedef struct {
    ProtoBufferHandle_t query_buffer;
    uint32_t query_offset;
    uint32_t query_length;
    ProtoBufferHandle_t output_buffer;
    uint32_t output_offset;
    uint32_t output_length;
    uint32_t page_index;
    uint32_t category;
    uint32_t sort;
    uint32_t kernel_abi;
} ProtoStoreCreateQueryRequest_t;

typedef struct {
    uint32_t token;
    ProtoBufferHandle_t response_buffer;
    uint32_t response_offset;
    uint32_t response_length;
    ProtoBufferHandle_t output_buffer;
    uint32_t output_offset;
    uint32_t output_length;
    uint32_t catalog_index;
    uint32_t screenshot_index;
    uint32_t strip_index;
} ProtoStoreValidateRequest_t;

typedef struct {
    uint32_t catalog_token;
    uint32_t catalog_index;
    ProtoHandle_t bundle_handle;
    ProtoHandle_t signature_handle;
    ProtoBufferHandle_t summary_buffer;
    uint32_t summary_offset;
    uint32_t summary_length;
    uint32_t flags;
} ProtoStoreInstallBeginRequest_t;

typedef struct {
    uint32_t token;
    ProtoBufferHandle_t summary_buffer;
    uint32_t summary_offset;
    uint32_t summary_length;
} ProtoStoreInstallJobRequest_t;

/* Output-only. The caller sizes the window; the kernel writes whole
 * ProtoStoreInstalledEntry_t records and reports the count in output_bytes. */
typedef struct {
    ProtoBufferHandle_t output_buffer;
    uint32_t output_offset;
    uint32_t output_length;
} ProtoStoreLedgerListRequest_t;

typedef struct {
    uint32_t op;
    union {
        ProtoStoreCreateQueryRequest_t create_query;
        ProtoStoreValidateRequest_t validate;
        ProtoStoreInstallBeginRequest_t install_begin;
        ProtoStoreInstallJobRequest_t install_job;
        ProtoStoreLedgerListRequest_t ledger_list;
    } data;
} ProtoStoreBrokerRequest_t;

typedef struct {
    uint32_t token;
    uint32_t status;
    int32_t result_code;
    uint32_t progress_bytes;
    uint32_t total_bytes;
    uint32_t catalog_serial;
    uint32_t item_count;
    uint32_t output_bytes;
} ProtoStoreBrokerResult_t;

typedef struct {
    uint32_t token;
    uint32_t query_hash;
    uint8_t nonce[PROTO_STORE_BROKER_NONCE_BYTES];
    char nonce_hex[PROTO_STORE_BROKER_NONCE_HEX_BYTES];
    uint8_t query_digest[PROTO_STORE_BROKER_NONCE_BYTES];
    uint8_t reserved[3];
} ProtoStoreQueryBinding_t;

typedef struct {
    void   *ptr;
    size_t  size;
    uint32_t flags;
    uint32_t generation;
} ProtoShmInfo_t;

typedef struct {
    char     path[PROTO_PATH_MAX];
    uint32_t flags;
} ProtoOpenRequest_t;

typedef struct {
    char path[PROTO_PATH_MAX];
} ProtoPathRequest_t;

typedef struct {
    ProtoHandle_t handle;
} ProtoCloseRequest_t;

typedef struct {
    ProtoHandle_t handle;
    void         *buffer;
    size_t        len;
} ProtoReadWriteRequest_t;

/* Positioned file I/O request.
 *
 * The offset is a 64-bit byte position carried as two 32-bit halves rather
 * than a uint64_t field. That is deliberate and must stay that way: this
 * struct is a member of the SysCallPacket_t union, the packet is embedded in
 * ProcessControlBlock_t, and a uint64_t member would raise the packet's
 * alignment from 4 to 8. That silently repads every PCB and moves the `data`
 * union within the packet, breaking the U-mode wire layout for any app
 * compiled against an older header. The halves keep the packet 4-byte
 * aligned; PROTO_POSITIONED_OFFSET() recombines them. */
typedef struct {
    ProtoHandle_t handle;
    void         *buffer;
    size_t        len;
    uint32_t      offset_lo;
    uint32_t      offset_hi;
} ProtoPositionedIoRequest_t;

#define PROTO_POSITIONED_OFFSET(req) \
    (((uint64_t)(req).offset_hi << 32) | (uint64_t)(req).offset_lo)

typedef struct {
    void    *buffer;
    size_t   len;
    uint32_t flags;
} ProtoBufferRegisterRequest_t;

typedef struct {
    ProtoBufferHandle_t buffer;
} ProtoBufferCloseRequest_t;

typedef struct {
    ProtoHandle_t       handle;
    ProtoBufferHandle_t buffer;
    size_t              offset;
    size_t              len;
} ProtoReadWriteBufferRequest_t;

typedef enum {
    PROTO_DIRENT_FILE = 1u,
    PROTO_DIRENT_DIRECTORY = 2u
} ProtoDirEntryType_t;

typedef struct {
    uint32_t type;
    size_t   size;
    char     name[PROTO_NAME_MAX];
} ProtoDirEntry_t;

typedef struct {
    bool     exists;
    bool     is_dir;
    size_t   size;
    uint32_t mtime;
} ProtoStat_t;

typedef struct {
    uint32_t seq;
    uint32_t uptime_ms;
    uint32_t live_processes;
    uint32_t cpu_total_x100;
    size_t   free_internal_bytes;
    size_t   free_spiram_bytes;
    uint32_t syscall_rate_x100;
    uint32_t denied_rate_x100;
    uint32_t ipc_rate_x100;
    uint32_t ui_latency_avg_us;
    uint32_t ui_latency_max_us;
} ProtoTelemetrySample_t;

typedef struct {
    uint32_t pid;
    uint32_t generation;
    uint32_t app_id;
    uint32_t state;
    uint32_t security_profile;
    uint32_t flags;
    uint32_t entry_kind;
    uint32_t loaded_trust;
    uint32_t loaded_image_bytes;
    uint32_t loaded_hash32;
    uint32_t loaded_object_count;
    uint32_t loaded_private_rx_bytes;
    uint32_t loaded_shared_rx_bytes;
    uint32_t loaded_rw_bytes;
    uint32_t uptime_ms;
    uint32_t cpu_x100;
    uint32_t stack_high_water;
    size_t   memory_current_bytes;
    size_t   memory_quota_bytes;
    size_t   memory_peak_bytes;
    uint32_t active_heap_regions;
    uint32_t syscall_count;
    uint32_t denied_syscall_count;
    uint32_t ipc_sent_count;
    uint32_t ipc_received_count;
    uint32_t sleep_count;
    uint32_t alloc_count;
    uint32_t free_count;
    char     name[PROTO_NAME_MAX];
} ProtoProcessSnapshot_t;

/* Canonical kernel service observation ABI. Lifecycle controls are
 * intentionally not part of this public read-only snapshot surface. */
#define PROTO_SERVICE_SNAPSHOT_MAX 12u
#define PROTO_SERVICE_NAME_MAX 24u
#define PROTO_SERVICE_DESCRIPTION_MAX 64u
#define PROTO_SERVICE_ERROR_MAX 64u
#define PROTO_SERVICE_DEPENDENCY_MAX 4u

typedef enum {
    PROTO_SERVICE_STOPPED = 0u,
    PROTO_SERVICE_STARTING,
    PROTO_SERVICE_RUNNING,
    PROTO_SERVICE_DEGRADED,
    PROTO_SERVICE_FAILED
} ProtoServiceState_t;

typedef enum {
    PROTO_SERVICE_HEALTH_UNKNOWN = 0u,
    PROTO_SERVICE_HEALTH_OK,
    PROTO_SERVICE_HEALTH_WARNING,
    PROTO_SERVICE_HEALTH_FAILED
} ProtoServiceHealth_t;

typedef enum {
    PROTO_SERVICE_BACKEND_PROCESS = 0u,
    PROTO_SERVICE_BACKEND_TASK,
    PROTO_SERVICE_BACKEND_TASK_GROUP,
    PROTO_SERVICE_BACKEND_SUBSYSTEM
} ProtoServiceBackend_t;

typedef enum {
    PROTO_SERVICE_RESTART_NEVER = 0u,
    PROTO_SERVICE_RESTART_ON_FAILURE,
    PROTO_SERVICE_RESTART_ON_UNHEALTHY
} ProtoServiceRestartMode_t;

typedef enum {
    PROTO_SERVICE_FLAG_ESSENTIAL        = (1u << 0),
    PROTO_SERVICE_FLAG_TRUSTED          = (1u << 1),
    PROTO_SERVICE_FLAG_RESTARTABLE      = (1u << 2),
    PROTO_SERVICE_FLAG_DESIRED_RUNNING  = (1u << 3),
    PROTO_SERVICE_FLAG_CPU_VALID        = (1u << 4),
    PROTO_SERVICE_FLAG_MEMORY_VALID     = (1u << 5),
    PROTO_SERVICE_FLAG_MEMORY_ESTIMATED = (1u << 6),
    PROTO_SERVICE_FLAG_HEALTH_VALID     = (1u << 7)
} ProtoServiceFlag_t;

typedef struct {
    uint32_t service_id;
    uint32_t generation;
    uint32_t state;
    uint32_t health;
    uint32_t backend;
    uint32_t restart_mode;
    uint32_t flags;
    uint32_t owner_pid;
    uint32_t owner_generation;
    uint32_t uptime_ms;
    uint32_t state_since_ms;
    uint32_t restart_count;
    uint32_t consecutive_failures;
    uint32_t last_health_ms;
    uint32_t last_healthy_ms;
    uint32_t health_latency_us;
    uint32_t cpu_x100;
    size_t   memory_current_bytes;
    size_t   memory_peak_bytes;
    size_t   memory_quota_bytes;
    int32_t  last_error;
    uint32_t last_error_ms;
    uint32_t required_dependency_count;
    uint32_t optional_dependency_count;
    uint32_t required_dependencies[PROTO_SERVICE_DEPENDENCY_MAX];
    uint32_t optional_dependencies[PROTO_SERVICE_DEPENDENCY_MAX];
    char     name[PROTO_SERVICE_NAME_MAX];
    char     description[PROTO_SERVICE_DESCRIPTION_MAX];
    char     health_detail[PROTO_SERVICE_ERROR_MAX];
    char     last_error_detail[PROTO_SERVICE_ERROR_MAX];
} ProtoServiceSnapshot_t;

typedef struct {
    uint32_t abi_version;
    uint32_t count;
    uint32_t sequence;
    uint32_t sampled_at_ms;
    ProtoServiceSnapshot_t services[PROTO_SERVICE_SNAPSHOT_MAX];
} ProtoServiceSnapshotList_t;

typedef struct {
    char     name[PROTO_NAME_MAX];
    uint64_t count;
    uint64_t total_us;
    uint32_t max_us;
    uint32_t avg_us;
} ProtoPerfSnapshot_t;

typedef struct {
    uint32_t abi_version;
    bool     cpu_supported;
    uint8_t  reserved[3];
    uint32_t sample_count;
    uint32_t process_count;
    ProtoTelemetrySample_t latest;
    ProtoTelemetrySample_t samples[PROTO_TELEMETRY_HISTORY_DEPTH];
    ProtoProcessSnapshot_t processes[PROTO_PROCESS_SNAPSHOT_MAX];
} ProtoSystemSnapshot_t;

typedef struct {
    uint32_t count;
    ProtoPerfSnapshot_t rows[PROTO_PERF_SNAPSHOT_MAX];
} ProtoPerfSnapshotList_t;

/* ── Storage accounting (/sys/storage) ────────────────────────────────────
 * Aggregate tier: volume totals plus per-namespace rollups keyed by
 * KernelNamespaceId_t. Deliberately carries NO file names and NO paths —
 * per-file detail is a separate admin-session-gated surface, because sizes
 * and names across /apps are a cross-app metadata side channel that the
 * observe capability alone must not open (KSAS INV-011/INV-026). */
/* Kernel-side roots are a fixed list of four (/apps /shared /log /etc); six
 * leaves slack without bloating the cached copy. Both this and the short name
 * field are deliberately tight: the snapshot cache lives in internal .bss,
 * which is the scarce memory on this board. */
#define PROTO_STORAGE_ROLLUP_MAX 6u
#define PROTO_STORAGE_ROLLUP_NAME_MAX 16u

/* Rollup is aggregate-only: /etc reports a single opaque total because
 * per-file sizes there disclose auth/ledger state (KSAS INV-006). */
#define PROTO_STORAGE_ROLLUP_OPAQUE  (1u << 0)
/* The bounded walk hit its entry/depth budget; bytes are a floor, not a total. */
#define PROTO_STORAGE_ROLLUP_PARTIAL (1u << 1)

typedef struct {
    uint64_t bytes;      /* true sum of file sizes in this area */
    /* Cluster-rounded footprint. Small files round up to a whole cluster, so
     * on a volume of many small files this is the figure that matches what
     * the area actually costs — and the only one that sums to the snapshot
     * total. Show this to users; `bytes` is the logical size. */
    uint64_t allocated;
    uint32_t namespace_id;
    uint32_t flags;
    uint32_t files;
    uint32_t dirs;
    char     name[PROTO_STORAGE_ROLLUP_NAME_MAX];
} ProtoStorageRollup_t;

/* Any rollup was truncated by its budget, so reachable_bytes is a floor and
 * unreachable_bytes is an upper bound. */
#define PROTO_STORAGE_SNAPSHOT_PARTIAL   (1u << 0)
/* Free space is at or below the kernel reserve: app writes are being refused
 * while kernel-critical writes still have headroom. */
#define PROTO_STORAGE_SNAPSHOT_RESERVED  (1u << 1)
/* FAT-reported used space materially exceeds the bytes reachable by walking
 * the tree: the volume has allocated clusters no directory entry references.
 * Only meaningful when PARTIAL is clear. */
#define PROTO_STORAGE_SNAPSHOT_UNREACHABLE (1u << 2)
/* The volume is not mounted; only the flags field is meaningful. */
#define PROTO_STORAGE_SNAPSHOT_UNMOUNTED (1u << 3)

/* ── Per-file detail (/sys/storage/files, admin tier) ─────────────────────
 * Names and sizes across /apps are a cross-app metadata side channel, so this
 * surface additionally requires ADMIN session authority (KSAS INV-014) on top
 * of the observe capability the aggregate tier needs. Entries carry logical
 * paths only — raw /fs paths never cross this boundary (INV-011) — and /etc
 * is never itemised, only summarised, because per-file sizes there disclose
 * auth and ledger state (INV-006). */
#define PROTO_STORAGE_FILE_NAME_MAX   28u
#define PROTO_STORAGE_FILE_PARENT_MAX 32u
#define PROTO_STORAGE_FILE_MAX        40u

/* Synthetic row standing in for an area that is never itemised (/etc). */
#define PROTO_STORAGE_FILE_AGGREGATE (1u << 0)

#define PROTO_STORAGE_FILE_OWNER_MAX 24u

typedef struct {
    uint32_t bytes;         /* true file size */
    uint32_t allocated;     /* cluster-rounded footprint */
    uint16_t namespace_id;  /* owning area */
    uint8_t  depth;
    uint8_t  flags;
    char     name[PROTO_STORAGE_FILE_NAME_MAX];
    char     parent[PROTO_STORAGE_FILE_PARENT_MAX]; /* logical, e.g. /apps/903 */
    /* Human-readable owner for files under /apps — the installed app's
     * display name, so a row reads "Tetris" instead of "app-c91503f1.elf".
     * Empty when the file has no owning app or the manifest is unreadable. */
    char     owner[PROTO_STORAGE_FILE_OWNER_MAX];
} ProtoStorageFileEntry_t;

/* The walk hit its entry budget; the ranking is over what it managed to see. */
#define PROTO_STORAGE_FILES_TRUNCATED (1u << 0)

typedef struct {
    uint32_t abi_version;
    uint32_t count;            /* entries retained (largest first) */
    uint32_t scanned;          /* entries the walk examined */
    uint32_t flags;
    uint32_t scan_duration_us;
    uint32_t reserved;
    ProtoStorageFileEntry_t entries[PROTO_STORAGE_FILE_MAX];
} ProtoStorageFileList_t;

typedef struct {
    uint64_t volume_total_bytes;
    uint64_t volume_free_bytes;
    uint64_t volume_used_bytes;   /* FAT-reported: total - free */
    uint64_t reserved_bytes;      /* kernel-only floor (see INV-028) */
    uint64_t available_bytes;     /* what an app may still consume */
    uint64_t reachable_bytes;     /* true sum of file sizes the walk reached */
    /* Cluster-rounded footprint of everything the walk reached. This, not
     * reachable_bytes, is what the FAT figures are comparable to. */
    uint64_t allocated_bytes;
    uint64_t unreachable_bytes;   /* used - allocated, clamped at zero */
    uint32_t abi_version;
    uint32_t flags;
    uint32_t epoch;               /* bumps on every storage mutation */
    uint32_t sampled_at_ms;
    uint32_t scan_age_ms;         /* age of the cached walk at sample time */
    uint32_t scan_duration_us;
    uint32_t reachable_files;
    uint32_t reachable_dirs;
    uint32_t cluster_bytes;
    uint32_t rollup_count;
    ProtoStorageRollup_t rollups[PROTO_STORAGE_ROLLUP_MAX];
} ProtoStorageSnapshot_t;

/* ── Memory accounting (/sys/memory) ─────────────────────────────────────
 * "Where did the RAM go", for the two RAM pools. Per pool:
 *
 *   physical_bytes   = fixed_bytes + heap_total_bytes
 *   heap_total_bytes = heap_free_bytes + sum(owner.bytes, non-FIXED rows)
 *   fixed_bytes      = sum(owner.bytes, FIXED rows)
 *
 * Every row's `bytes` is exclusive, so a pool's rows plus its free bytes add
 * up to its physical size with nothing counted twice. `nested_bytes` never
 * enters those sums: on a CONTAINER row it is how much of the container is
 * handed out; on a PROCESS row it is what that app holds INSIDE a container
 * (its sandbox stacks/heap, its XIP RW slot), which the container row already
 * counts. Same observe tier as /sys/telemetry: names, sizes, no addresses. */
#define PROTO_MEMORY_SNAPSHOT_VERSION 1u
#define PROTO_MEMORY_OWNER_MAX        80u
#define PROTO_MEMORY_OWNER_NAME_MAX   16u

#define PROTO_MEMORY_POOL_SRAM  0u
#define PROTO_MEMORY_POOL_PSRAM 1u
#define PROTO_MEMORY_POOL_COUNT 2u

#define PROTO_MEMORY_OWNER_FIXED     1u /* carved out at boot, never heap */
#define PROTO_MEMORY_OWNER_CONTAINER 2u /* kernel arena apps allocate from */
#define PROTO_MEMORY_OWNER_KERNEL    3u /* kernel subsystem buffers */
#define PROTO_MEMORY_OWNER_SERVICE   4u
#define PROTO_MEMORY_OWNER_PROCESS   5u
#define PROTO_MEMORY_OWNER_OTHER     6u /* heap in use that no row claims */

/* Rows ran out; the dropped bytes were folded into the OTHER rows. */
#define PROTO_MEMORY_SNAPSHOT_TRUNCATED (1u << 0)

typedef struct {
    uint32_t bytes;
    uint32_t nested_bytes;
    uint32_t pid;          /* PROCESS rows only */
    uint8_t  pool;         /* PROTO_MEMORY_POOL_* */
    uint8_t  kind;         /* PROTO_MEMORY_OWNER_* */
    uint8_t  reserved[2];
    char     name[PROTO_MEMORY_OWNER_NAME_MAX];
} ProtoMemoryOwner_t;

typedef struct {
    uint32_t physical_bytes;
    uint32_t fixed_bytes;
    uint32_t heap_total_bytes;
    uint32_t heap_free_bytes;
    uint32_t heap_largest_free_bytes;
    uint32_t heap_min_free_bytes;   /* low-water mark since boot */
} ProtoMemoryPool_t;

typedef struct {
    uint32_t abi_version;
    uint32_t flags;
    uint32_t sampled_at_ms;
    uint32_t owner_count;
    ProtoMemoryPool_t pools[PROTO_MEMORY_POOL_COUNT];
    /* SRAM that ordinary malloc will not hand out, kept for DMA and
     * internal-only driver requests. Part of pools[SRAM].heap_free_bytes. */
    uint32_t sram_driver_reserve_bytes;
    /* The two small SRAM side-heaps (TCM and LP SRAM). Already included in
     * pools[SRAM]; listed so the 768 KiB main SRAM can be told apart. */
    uint32_t tcm_total_bytes;
    uint32_t tcm_free_bytes;
    uint32_t lp_total_bytes;
    uint32_t lp_free_bytes;
    ProtoMemoryOwner_t owners[PROTO_MEMORY_OWNER_MAX];
} ProtoMemorySnapshot_t;

/* Kernel-enforced networking ceilings (Phase 0a).
 *
 * These are the values the KERNEL clamps to; they are published here so the SDK
 * never advertises a bound the kernel will not honour. A clamp always produces a
 * SHORT TRANSFER, never an error: partial reads and partial writes remain part
 * of the contract and every caller must loop.
 *
 * Timeout-zero semantics are deliberately ASYMMETRIC and must not be unified:
 *   - read/write: timeout_ms == 0 means a NONBLOCKING POLL.
 *   - connect:    timeout_ms == 0 means "use the default connect ceiling".
 * See KSAS section 10.2.
 */
#define PROTO_NET_TRANSFER_TIMEOUT_MAX_MS 500u
#define PROTO_NET_TRANSFER_BYTES_MAX      8192u
#define PROTO_NET_CONNECT_TIMEOUT_MAX_MS  15000u
#define PROTO_NET_LISTEN_BACKLOG_MAX       4u
#define PROTO_NET_POLL_ITEMS_MAX           16u

/* Inbound port policy (Phase 3).
 *
 * Ports below PROTO_NET_LISTEN_PORT_MIN are reserved to the kernel and to the
 * well-known services an application must never be able to impersonate. An app
 * asking for one is REJECTED, never silently rebound onto a different port:
 * silently moving a listener would let an app believe it owns a service port it
 * does not, and would make a port conflict look like success.
 *
 * Port 0 stays legal and means "ephemeral" -- the kernel asks lwIP to pick a
 * free high port and reports the bound port back. That is the only path that
 * may yield a port the app did not name.
 *
 * PROTO_NET_LISTEN_PORT_LEASE_MAX bounds the kernel's lease table. One lease is
 * one live listener, so it matches the kernel socket slot count. */
#define PROTO_NET_LISTEN_PORT_MIN          1024u
/* Start of lwIP's TCP ephemeral range (TCP_LOCAL_PORT_RANGE_START = 0xc000 in
 * lwip/src/core/tcp.c). Ports at or above this are transient by definition: the
 * stack hands them out itself, no service can hold a stable identity there, and
 * an app that names one is therefore not squatting anything. The declared-port
 * rule governs only the REGISTERED range below this line, which is where a
 * squatted port would actually mean something (25565 being the case in point).
 * Naming an ephemeral port stays legal so an app can re-bind the port it was
 * just handed -- which is exactly how the Phase 2 acceptance app proves a closed
 * listener released both its descriptor and its lease. Conflicts in this range
 * are still refused by the lease table. Keep in step with lwIP if that changes. */
#define PROTO_NET_LISTEN_PORT_EPHEMERAL_MIN 49152u
#define PROTO_NET_LISTEN_PORT_LEASE_MAX    8u
/* RiskCraft's Minecraft port -- the one inbound port this project intends to
 * authorize. Named here so the manifest, the tests, and the docs cannot drift
 * apart on it. */
#define PROTO_NET_LISTEN_PORT_MINECRAFT    25565u

/* Per-process socket quotas (Phase 4).
 *
 * The kernel has WIFI_SOCKET_MAX (8) socket slots and they are a GLOBAL pool,
 * while handles are per-process -- so before these quotas one application could
 * take every slot and leave every other app unable to open a connection. There
 * is no kernel-side socket consumer today (Wi-Fi transport is SDIO, not lwIP),
 * so the starvation these bound is app-against-app.
 *
 * SOCKETS_PER_PROCESS_MAX is the backstop: 6 of 8, so at least two slots always
 * remain for other applications no matter what one app does. It counts only
 * LIVE slots the process owns. A CLOSING slot is deliberately NOT counted --
 * the app has already released it and its final close is kernel cleanup, so
 * charging it would make a quota failure depend on teardown timing.
 *
 * The role caps below encode the plan's stated policy directly. RiskCraft needs
 * 1 listener + 4 clients = 5. The Phase 2 acceptance harness peaks at exactly
 * 6 (1 listener + 4 accepted + 1 recycled outbound client), which is why the
 * backstop is 6 and not 5 -- see the coupling test in
 * tests/test_net_phase4_contract.py before lowering it. */
#define PROTO_NET_SOCKETS_PER_PROCESS_MAX   6u
#define PROTO_NET_LISTENERS_PER_PROCESS_MAX 1u
#define PROTO_NET_ACCEPTED_PER_LISTENER_MAX 4u

typedef enum {
    PROTO_NET_STATUS = 1u,
    PROTO_NET_TCP_CONNECT,
    PROTO_NET_TCP_READ,
    PROTO_NET_TCP_WRITE,
    PROTO_NET_TCP_CLOSE,
    PROTO_NET_TCP_LISTEN,
    PROTO_NET_TCP_ACCEPT,
    PROTO_NET_POLL,
    PROTO_NET_TCP_SHUTDOWN
} ProtoNetOp_t;

typedef enum {
    PROTO_NET_EVENT_READABLE = 1u << 0,
    PROTO_NET_EVENT_WRITABLE = 1u << 1,
    PROTO_NET_EVENT_ERROR    = 1u << 2
} ProtoNetEvent_t;

typedef enum {
    PROTO_NET_SHUTDOWN_READ = 0u,
    PROTO_NET_SHUTDOWN_WRITE,
    PROTO_NET_SHUTDOWN_BOTH
} ProtoNetShutdown_t;

/* Poll arrays live in registered buffers, never in SysCallPacket_t. `events`
 * is caller input; `revents` is overwritten by the kernel. */
typedef struct {
    ProtoHandle_t handle;
    uint16_t      events;
    uint16_t      revents;
} ProtoNetPollItem_t;

typedef struct {
    ProtoHandle_t handle;
    uint32_t      ipv4_addr; /* host byte order */
    uint16_t      port;      /* host byte order */
    uint16_t      reserved;
} ProtoNetAcceptResult_t;

typedef struct {
    ProtoHandle_t handle;
    uint16_t      port;      /* actual bound port, host byte order */
    uint16_t      reserved;
} ProtoNetListenResult_t;

typedef enum {
    PROTO_NET_STATE_DISABLED = 0u,
    PROTO_NET_STATE_ENABLED,
    PROTO_NET_STATE_CONNECTED,
    PROTO_NET_STATE_ERROR
} ProtoNetState_t;

typedef struct {
    uint32_t state;
    uint32_t enabled;
    uint32_t connected;
    int32_t  last_error;
    char     ssid[PROTO_NET_SSID_MAX + 1u];
} ProtoNetStatus_t;

typedef struct {
    uint32_t            op;
    ProtoHandle_t      handle;
    ProtoBufferHandle_t buffer;
    size_t             offset;
    size_t             len;
    uint16_t           port;
    uint16_t           timeout_ms;
    uint16_t           backlog;
    uint16_t           flags;
    uint32_t           count;
    char               host[PROTO_NET_HOST_MAX];
} ProtoNetRequest_t;

typedef struct {
    uint32_t abi_version;
    uint32_t pid;
    uint32_t app_id;
    uint32_t app_generation;
    uint32_t entry_kind;
    uint32_t security_profile;
    uint32_t granted_permissions;
    uint32_t capabilities;
    uint32_t uptime_ms;
    uint64_t monotonic_us;
    uint64_t epoch_seconds;
    uint32_t time_synced;
    uint32_t sntp_started;
    uint32_t live_processes;
    size_t   free_internal_bytes;
    size_t   free_spiram_bytes;
} ProtoKernelInfo_t;

/* Shared Settings/Weather preference. Only the built-in Weather app can use
 * PROTO_SYS_TIME_LOCATION; callers cannot name a storage path or another app. */
#define PROTO_LOCATION_QUERY_MAX 48u
#define PROTO_LOCATION_LABEL_MAX 48u
#define PROTO_LOCATION_TZ_MAX    48u
#define PROTO_UNITS_IMPERIAL 0u
#define PROTO_UNITS_METRIC   1u
#define PROTO_TIME_LOCATION_GET     0u
#define PROTO_TIME_LOCATION_RESOLVE 1u
typedef struct {
    bool     configured;
    bool     resolved;
    uint32_t generation;
    uint32_t units;
    char     query[PROTO_LOCATION_QUERY_MAX + 1u];
    char     label[PROTO_LOCATION_LABEL_MAX + 1u];
    char     timezone[PROTO_LOCATION_TZ_MAX + 1u];
    int32_t  latitude_e6;
    int32_t  longitude_e6;
} ProtoTimeLocationConfig_t;

typedef struct {
    uint32_t op;
    void    *buffer;
    size_t   len;
} ProtoTimeLocationRequest_t;

typedef struct {
    void  *buffer;
    size_t len;
} ProtoKernelInfoRequest_t;

typedef struct {
    size_t   quota_bytes;
    size_t   live_requested_bytes;
    size_t   region_bytes;
    uint32_t active_regions;
    uint32_t live_blocks;
    uint32_t free_blocks;
    size_t   free_total_bytes;
    size_t   largest_free_bytes;
    uint32_t fragmentation_x100;
    uint32_t heap_pressure_x100;
    uint32_t tlsf_nonempty_bins;
    uint32_t metadata_used;
    uint32_t metadata_free;
    bool     compromised;
    bool     quarantined;
    bool     deterministic_layout;
    uint8_t  reserved[1];
} ProtoAllocatorSnapshot_t;

typedef struct {
    void  *buffer;
    size_t len;
} ProtoAllocatorSnapshotRequest_t;

typedef enum {
    PROTO_PROCESS_CONTROL_TERMINATE = 1u
} ProtoProcessControlOp_t;

typedef struct {
    uint32_t op;
    uint32_t pid;
    uint32_t generation;
} ProtoProcessControlRequest_t;

typedef struct {
    ProtoHandle_t handle;
    uint32_t      op;
    union {
        struct {
            KernelGpioMode_t mode;
            KernelGpioPull_t pull;
        } gpio_configure;
        struct {
            uint32_t reserved;
        } shm;
        ProtoAudioConfig_t audio_configure;
        struct {
            uint32_t volume_pct;
        } audio_volume;
        struct {
            uint32_t frequency_hz;
            uint32_t duration_ms;
            uint32_t volume_pct;
        } audio_tone;
    } arg;
} ProtoIoctlRequest_t;

typedef enum {
    PROTO_GRAPHICS_OPEN_SURFACE = 0,
    PROTO_GRAPHICS_SUBMIT_COMMANDS,
    PROTO_GRAPHICS_SCROLL,
    PROTO_GRAPHICS_CLOSE_SURFACE,
    PROTO_GRAPHICS_GAME_OPEN_SURFACE,
    PROTO_GRAPHICS_GAME_UPLOAD_ASSET,
    PROTO_GRAPHICS_GAME_SUBMIT_FRAME,
    PROTO_GRAPHICS_GAME_DESTROY_ASSET,
    /* ABI 13 append-only Game2D operations. */
    PROTO_GRAPHICS_GAME_UPLOAD_PALETTE,
    PROTO_GRAPHICS_GAME_SUBMIT_FRAME_V2,
    PROTO_GRAPHICS_GAME_DESTROY_PALETTE,
    /* Returns the PROTO_GAME2D_CAP_* mask. Older ABI-19 kernels reject this
     * op, which gives newer apps an explicit compatibility fallback. */
    PROTO_GRAPHICS_GAME_QUERY_CAPABILITIES,
    /* Protocol-versioned Game3D control in registered buffers. Append-only;
     * the outer Game2D packet stays 60 bytes and old operation IDs stay fixed. */
    PROTO_GRAPHICS_GAME3D
} ProtoGraphicsOp_t;

typedef enum {
    PROTO_UI_CMD_CLEAR = 0,
    PROTO_UI_CMD_RECT,
    PROTO_UI_CMD_LABEL,
    PROTO_UI_CMD_CIRCLE,
    PROTO_UI_CMD_SCROLL_CONTAINER,
    PROTO_UI_CMD_SEPARATOR
} ProtoUiCommandOp_t;

typedef enum {
    PROTO_UI_ALIGN_LEFT = 0,
    PROTO_UI_ALIGN_CENTER,
    PROTO_UI_ALIGN_RIGHT
} ProtoUiTextAlign_t;

/* Optional RECT/CIRCLE background gradients. The second RGB888 colour is
 * stored in text[0..3] so the command wire size stays compatible with ABI 12;
 * text is otherwise unused by shape commands. */
#define PROTO_UI_FLAG_GRADIENT_HOR 0x01u
#define PROTO_UI_FLAG_GRADIENT_VER 0x02u

typedef struct {
    uint8_t op;
    uint8_t id;
    uint8_t parent_id;
    uint8_t flags;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t radius;
    uint8_t opacity;
    uint8_t align;
    uint16_t text_size; /* PROTO_UI_TEXT_* for labels; 0 keeps default font. */
    uint32_t color;
    char text[PROTO_UI_TEXT_MAX];
} ProtoUiCommand_t;

typedef uint32_t ProtoGameAssetHandle_t;
typedef uint32_t ProtoGamePaletteHandle_t;
typedef uint32_t ProtoGameCapabilities_t;

#define PROTO_GAME2D_CAP_COMMANDS_V2       (1u << 0)
#define PROTO_GAME2D_CAP_INDEXED_ASSETS    (1u << 1)
#define PROTO_GAME2D_CAP_A8_MASKS          (1u << 2)
#define PROTO_GAME2D_CAP_CACHED_LAYERS     (1u << 3)
#define PROTO_GAME2D_CAP_MASK_SOURCE_RECTS (1u << 4)
#define PROTO_GAME2D_CAP_ARGB4444          (1u << 5)
#define PROTO_GAME2D_CAP_SMOOTH_SHAPES     (1u << 6)
#define PROTO_GAME2D_CAPABILITIES_CURRENT  \
    (PROTO_GAME2D_CAP_COMMANDS_V2 | PROTO_GAME2D_CAP_INDEXED_ASSETS | \
     PROTO_GAME2D_CAP_A8_MASKS | PROTO_GAME2D_CAP_CACHED_LAYERS | \
     PROTO_GAME2D_CAP_MASK_SOURCE_RECTS | PROTO_GAME2D_CAP_ARGB4444 | \
     PROTO_GAME2D_CAP_SMOOTH_SHAPES)

typedef enum {
    PROTO_GAME2D_FORMAT_RGB565 = 1u,
    /* INDEX4 packs the even pixel in the high nibble and the odd pixel in the
     * low nibble. INDEX8 stores one palette index per byte. */
    PROTO_GAME2D_FORMAT_INDEX4 = 2u,
    PROTO_GAME2D_FORMAT_INDEX8 = 3u,
    /* One byte of coverage per pixel. A MASK command supplies the RGB565
     * colour; the asset contains no palette or persistent user pointer. */
    PROTO_GAME2D_FORMAT_A8 = 4u,
    /* Host-endian 0xARGB nibbles. Unlike a colour key, the four-bit alpha
     * channel preserves soft coloured edges while keeping assets at two
     * bytes per pixel. Asset flags, color_key and palette must be zero. */
    PROTO_GAME2D_FORMAT_ARGB4444 = 5u
} ProtoGamePixelFormat_t;

typedef enum {
    PROTO_GAME2D_ASSET_COLOR_KEY = 1u << 0
} ProtoGameAssetFlag_t;

typedef struct {
    /* ABI 12 prefix: do not reorder. For indexed formats, color_key's low
     * byte is the transparent palette index when COLOR_KEY is set. */
    uint16_t width;
    uint16_t height;
    uint32_t format;
    uint32_t flags;
    uint16_t color_key;
    uint16_t reserved;
    /* ABI 13: required default LUT for indexed assets. A V2 sprite may select
     * another compatible palette without duplicating the pixel indices. */
    ProtoGamePaletteHandle_t palette;
} ProtoGameAssetDesc_t;

typedef enum {
    PROTO_GAME2D_CMD_CLEAR = 0,
    PROTO_GAME2D_CMD_FILL_RECT,
    PROTO_GAME2D_CMD_SPRITE,
    PROTO_GAME2D_CMD_TEXT8
} ProtoGameDrawOp_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} ProtoRect_t;

typedef struct {
    uint8_t op;
    uint8_t flags;
    uint16_t reserved;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint32_t color;
    ProtoGameAssetHandle_t asset;
    uint16_t src_x;
    uint16_t src_y;
    char text[PROTO_GAME2D_TEXT_MAX];
} ProtoGameDrawCommand_t;

/* ABI 13 compact command. At 32 bytes this doubles the command ceiling while
 * keeping the worst-case trusted per-frame copy close to ABI 12. Text lives in
 * a separately registered byte buffer and is selected by offset/length. */
#include "proto_game2d_commands.h"

#ifdef __cplusplus
static_assert(offsetof(ProtoGameAssetDesc_t, palette) == 16u,
              "Game2D asset ABI 12 prefix changed");
static_assert(sizeof(ProtoGameAssetDesc_t) == 20u,
              "Game2D asset descriptor ABI size changed");
static_assert(sizeof(ProtoGameDrawCommandV2_t) == 32u,
              "Game2D V2 command must stay compact");
#else
_Static_assert(offsetof(ProtoGameAssetDesc_t, palette) == 16u,
               "Game2D asset ABI 12 prefix changed");
_Static_assert(sizeof(ProtoGameAssetDesc_t) == 20u,
               "Game2D asset descriptor ABI size changed");
_Static_assert(sizeof(ProtoGameDrawCommandV2_t) == 32u,
               "Game2D V2 command must stay compact");
#endif

typedef struct {
    uint32_t op;
    uint32_t surface_id;
    union {
        struct {
            char title[PROTO_NAME_MAX];
        } open;
        struct {
            ProtoBufferHandle_t command_buffer;
            uint32_t command_count;
        } submit;
        struct {
            int16_t y;
            bool animated;
        } scroll;
        struct {
            char title[PROTO_NAME_MAX];
            uint16_t bg_color;
            uint16_t reserved;
        } game_open;
        struct {
            ProtoBufferHandle_t pixel_buffer;
            ProtoGameAssetDesc_t desc;
        } game_asset;
        struct {
            ProtoBufferHandle_t draw_buffer;
            uint32_t draw_count;
            ProtoBufferHandle_t dirty_buffer;
            uint32_t dirty_count;
            uint32_t frame_flags;
        } game_frame;
        struct {
            ProtoGameAssetHandle_t asset;
        } game_destroy;
        struct {
            ProtoBufferHandle_t color_buffer;
            uint16_t color_count;
            uint16_t reserved;
        } game_palette;
        struct {
            ProtoBufferHandle_t draw_buffer;
            uint32_t draw_count;
            ProtoBufferHandle_t text_buffer;
            uint32_t text_bytes;
            ProtoBufferHandle_t dirty_buffer;
            uint32_t dirty_count;
            uint32_t frame_flags;
        } game_frame_v2;
        struct {
            ProtoGamePaletteHandle_t palette;
        } game_palette_destroy;
        struct {
            uint32_t version;
            ProtoBufferHandle_t control_buffer;
            uint32_t control_offset,control_bytes;
        } game3d;
    } data;
} ProtoGraphicsRequest_t;

#ifdef __cplusplus
static_assert(sizeof(ProtoGraphicsRequest_t) == 60u,
              "graphics request ABI size changed");
#else
_Static_assert(sizeof(ProtoGraphicsRequest_t) == 60u,
               "graphics request ABI size changed");
#endif

#include "proto_foundation_abi.h"

typedef struct {
    SysCallType_t call_type;
    uint32_t      app_id;
    uint32_t      auth_token;
    union {
        char   print_payload[64];
        size_t requested_memory_size;
        void  *free_ptr;
        struct {
            uint32_t     target_pid;
            IpcMessage_t message;
        } ipc_send;
        struct {
            uint32_t timeout_ms;
        } ipc_recv;
        struct {
            uint32_t duration_ms;
        } sleep;
        struct {
            uint32_t deadline_us;
        } sleep_until;
        KernelGpioRequest_t gpio;
        ProtoOpenRequest_t open;
        ProtoPathRequest_t path;
        ProtoCloseRequest_t close;
        ProtoReadWriteRequest_t rw;
        ProtoPositionedIoRequest_t prw;
        ProtoBufferRegisterRequest_t buffer_register;
        ProtoBufferCloseRequest_t buffer_close;
        ProtoReadWriteBufferRequest_t rw_buf;
        ProtoIoctlRequest_t ioctl;
        ProtoGraphicsRequest_t graphics;
        ProtoDlOpenRequest_t dlopen;
        ProtoDlSymRequest_t dlsym;
        ProtoDlCloseRequest_t dlclose;
        ProtoAllocatorSnapshotRequest_t allocator_snapshot;
        ProtoFileDialogRequest_t file_dialog;
        ProtoNetRequest_t net;
        ProtoKernelInfoRequest_t kernel_info;
        ProtoTimeLocationRequest_t time_location;
        ProtoTrustedPromptRequest_t trusted_prompt;
        ProtoUiExitRequest_t ui_exit;
        ProtoKeyStateRequest_t key_state;
        ProtoPointerStateRequest_t pointer_state;
        ProtoGamepadStateRequest_t gamepad_state;
        ProtoThemeRequest_t theme;
        ProtoSoundRequest_t sound;
        ProtoThreadConfig_t thread_create;
        ProtoThreadLifecycleRequest_t thread_lifecycle;
        /* All three stay well under ProtoPathRequest_t, so the resource API
         * cannot grow the wire packet. Bulk bytes move through a registered
         * buffer, never through the packet. */
        ProtoResourceOpenRequest_t resource_open;
        ProtoResourceReadRequest_t resource_read;
        ProtoResourceCloseRequest_t resource_close;
        ProtoStoreBrokerRequest_t store;
        ProtoFoundationRequest_t foundation;
    } data;
} SysCallPacket_t;

/* The syscall packet is the U-mode wire format AND is embedded by value in
 * ProcessControlBlock_t. Its alignment therefore fixes both the offset of
 * `data` within the packet and the offset of every PCB field that follows the
 * embedded copy. Adding any 8-byte-aligned member (uint64_t, double, or a
 * struct containing one) to the union above silently repads both, breaking
 * apps compiled against an older header and moving kernel PCB fields for no
 * stated reason. Carry 64-bit quantities as two 32-bit halves instead; see
 * ProtoPositionedIoRequest_t. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(_Alignof(SysCallPacket_t) == 4u,
               "SysCallPacket_t must stay 4-byte aligned: an 8-byte-aligned "
               "union member repads ProcessControlBlock_t and shifts the "
               "U-mode wire layout");
_Static_assert(sizeof(SysCallPacket_t) == 160u,
               "ABI 18 must not grow SysCallPacket_t or shift PCB fields");
_Static_assert(sizeof(ProtoNetRequest_t) == 96u,
               "ABI 18 network request wire size changed");
_Static_assert(sizeof(ProtoNetPollItem_t) == 8u,
               "ABI 18 poll item wire size changed");
_Static_assert(sizeof(ProtoNetAcceptResult_t) == 12u,
               "ABI 18 accept result wire size changed");
_Static_assert(sizeof(ProtoNetRequest_t) <= sizeof(ProtoPathRequest_t),
               "network scalars must stay below the fixed path packet arm; "
               "put variable arrays in registered buffers");
/* The ioctl arm carries per-device argument structs and so is the member most
 * likely to grow by accident (adding /dev/audio widened it from 8 to 16
 * bytes). Growing the packet itself is what breaks apps built against an older
 * header, so pin the ioctl arm below a member whose size is fixed by
 * PROTO_PATH_MAX: while this holds, a new ioctl argument cannot change
 * sizeof(SysCallPacket_t) at all. If a device genuinely needs a larger
 * argument, pass it through a registered buffer rather than relaxing this. */
_Static_assert(sizeof(ProtoIoctlRequest_t) <= sizeof(ProtoPathRequest_t),
               "ProtoIoctlRequest_t must not become the largest SysCallPacket_t "
               "union member: that grows the U-mode wire packet and breaks "
               "binaries compiled against an older proto_abi.h");
_Static_assert(sizeof(ProtoStoreBrokerRequest_t) == 44u,
               "Store broker request ABI layout changed");
_Static_assert(sizeof(ProtoStoreBrokerResult_t) == 32u,
               "Store broker result ABI layout changed");
_Static_assert(sizeof(ProtoStoreQueryBinding_t) == 60u,
               "Store query binding ABI layout changed");
_Static_assert(sizeof(ProtoStoreBrokerRequest_t) <= sizeof(ProtoPathRequest_t),
               "Store broker scalars must not grow SysCallPacket_t");
_Static_assert(sizeof(ProtoKeyState_t) == 76u,
               "ABI 20 key state wire layout changed");
_Static_assert(sizeof(ProtoKeyStateRequest_t) <= sizeof(ProtoPathRequest_t),
               "key state request must not grow SysCallPacket_t");
_Static_assert(sizeof(ProtoPointerState_t) == 28u,
               "ABI 25 pointer state wire layout changed");
_Static_assert(sizeof(ProtoPointerStateRequest_t) <= sizeof(ProtoPathRequest_t),
               "pointer state request must not grow SysCallPacket_t");
_Static_assert(sizeof(ProtoGamepadState_t) == 40u,
               "ABI 26 gamepad state wire layout changed");
_Static_assert(sizeof(ProtoGamepadStateRequest_t) <= sizeof(ProtoPathRequest_t),
               "gamepad state request must not grow SysCallPacket_t");
_Static_assert(sizeof(ProtoTheme_t) == 188u,
               "ABI 22 theme wire layout changed");
_Static_assert(sizeof(ProtoThemeRequest_t) <= sizeof(ProtoPathRequest_t),
               "theme request must not grow SysCallPacket_t");
_Static_assert(sizeof(ProtoSoundRequest_t) == 4u,
               "ABI 23 sound request wire layout changed");
/* ipc_send is the packet's largest arm; staying below it is what keeps the
 * 160-byte assert above true for apps built against this header. */
_Static_assert(sizeof(ProtoFoundationRequest_t) <
                   sizeof(((SysCallPacket_t *)0)->data.ipc_send),
               "ABI 24 foundation request must not grow SysCallPacket_t");
_Static_assert(sizeof(ProtoFoundationResult_t) <= sizeof(IpcMessage_t),
               "ABI 24 foundation result must not grow SysCallResponse_t");
#endif

typedef struct {
    bool success;
    union {
        void        *allocated_ptr;
        int          return_code;
        ProtoHandle_t handle;
        ProtoBufferHandle_t buffer_handle;
        uint32_t     ui_surface_id;
        ProtoGameAssetHandle_t game_asset;
        ProtoGamePaletteHandle_t game_palette;
        size_t       bytes_transferred;
        uint32_t     time_us;
        ProtoStat_t  stat;
        ProtoShmInfo_t shm;
        IpcMessage_t received_msg;
        KernelGpioResult_t gpio;
        ProtoAudioStat_t audio;
        ProtoDlResult_t dl;
        ProtoFileDialogResult_t file_dialog;
        ProtoTrustedPromptResult_t trusted_prompt;
        ProtoNetStatus_t net_status;
        ProtoNetAcceptResult_t net_accept;
        ProtoNetListenResult_t net_listen;
        ProtoThreadHandle_t thread_handle;
        ProtoResourceOpenResult_t resource_open;
        ProtoResourceChunkInfo_t resource_chunk;
        ProtoStoreBrokerResult_t store;
        ProtoFoundationResult_t foundation;
    } result;
} SysCallResponse_t;

#define SECURITY_TEST_PID 27u
#define GPIO_TEST_PID     45u
#define SHM_TEST_PID      46u
#define AUDIO_TEST_PID    50u
#define LIFETIME_TEST_PID 49u
#define NET_LOCK_TEST_PID 51u
#define NET_PHASE2_TEST_PID 52u
#define NET_PHASE3_TEST_PID 53u
#define NET_PORT_TEST_PID   54u
#define NET_QUOTA_TEST_PID  55u
#define SP_PIVOT_TEST_PID 13u
#define BENCH_TEST_PID    18u
#define HEAP_STRESS_TEST_PID 19u
#define HEAP_SOAK_TEST_PID   20u
#define HEAP_FAULT_TEST_PID  21u
#define ALLOCATOR_BENCH_TEST_PID 22u
#define DUALCORE_BENCH_PID 32u
#define COREBENCH_PID      33u
#define MMODE_HARDENING_TEST_PID 28u
/* 29u (was MMODE_STACK_GUARD_TEST_PID) retired with the guard-word system. */
#define MMODE_STACK_CANARY_TEST_PID 30u
#define MMODE_HEAP_AUDIT_TEST_PID 31u
#define STORE_PID          35u
#define PETSTUDIO_PID      34u
#define WAVEBENCH_PID      36u
#define WAITBENCH_PID      37u
#define BOOT_SPLASH_PID    38u
#define POCKET_PID         39u
#define PIE_PROBE_PID      56u
#define PIE_DENIED_PROBE_PID 57u
#define PIE_UMODE_BENCH_PID 58u
#define PIE_UMODE_SYSCALLS_PID 59u
#define PIE_CTX_APP_PID    60u
#define PIE_PMP_PROBE_PID  61u
#define PIE_SRAM_BENCH_PID 62u
#define PIE_PSRAM_BENCH_PID 63u
/* Network stack plan harness: one body, U-mode and M-mode entries. */
#define NET_STACK_TEST_PID   64u
#define NET_STACK_TEST_M_PID 65u
#define NET_STACK_VICTIM_PID   66u
#define NET_STACK_VICTIM_M_PID 67u
#define ZB_PROBE_PID       68u
#define KERNEL_TEST_GPIO_PIN 21

#define KERNEL_UMODE_SYSCALL_MAGIC 0x4F535543u
