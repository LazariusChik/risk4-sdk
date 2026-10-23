#pragma once
#include <stdint.h>

/* ABI 24 additions. All wire quantities have four-byte alignment. Absolute
 * deadlines are monotonic microseconds; UINT64_MAX means no deadline and
 * zero is a nonblocking probe. Existing packets and response sizes stay fixed. */
#define PROTO_FOUNDATION_ABI_VERSION 24u
#define PROTO_WAIT_ITEMS_MAX 8u
#define PROTO_RANDOM_BYTES_MAX 256u
#define PROTO_PIPE_CAPACITY 512u
#define PROTO_RENAME_NOREPLACE 1u
#define PROTO_CLOCK_UTC_VALID 1u
#define PROTO_CLOCK_UTC_SYNCED 2u
#define PROTO_WAIT_READABLE 1u
#define PROTO_WAIT_WRITABLE 2u
#define PROTO_WAIT_ERROR 4u
#define PROTO_WAIT_CLOSED 8u
#define PROTO_TRANSFER_MOVE 1u
#define PROTO_REPLACE_RECOVERY_RETAINED 1u
#define PROTO_REPLACE_COMMITTED 2u
#define PROTO_TRUSTED_JOB_START 6u

typedef enum {
    PROTO_FOUNDATION_RENAME = 1,
    PROTO_FOUNDATION_RMDIR,
    PROTO_FOUNDATION_FSTAT,
    PROTO_FOUNDATION_FTRUNCATE,
    PROTO_FOUNDATION_FSYNC,
    PROTO_FOUNDATION_REPLACE,
    PROTO_FOUNDATION_EVENT_CREATE,
    PROTO_FOUNDATION_EVENT_SET,
    PROTO_FOUNDATION_EVENT_RESET,
    PROTO_FOUNDATION_SEMAPHORE_CREATE,
    PROTO_FOUNDATION_SEMAPHORE_POST,
    PROTO_FOUNDATION_SEMAPHORE_WAIT,
    PROTO_FOUNDATION_CLOSE,
    PROTO_FOUNDATION_WAIT_MANY,
    PROTO_FOUNDATION_CLOCK,
    PROTO_FOUNDATION_RANDOM,
    PROTO_FOUNDATION_DISCOVERY,
    PROTO_FOUNDATION_TIMER_CREATE,
    PROTO_FOUNDATION_TIMER_ARM,
    PROTO_FOUNDATION_TIMER_READ,
    PROTO_FOUNDATION_JOB_START,
    PROTO_FOUNDATION_JOB_STATUS,
    PROTO_FOUNDATION_JOB_CLOSE,
    PROTO_FOUNDATION_PIPE_CREATE,
    PROTO_FOUNDATION_PIPE_READ,
    PROTO_FOUNDATION_PIPE_WRITE,
    PROTO_FOUNDATION_TRANSFER_OFFER,
    PROTO_FOUNDATION_TRANSFER_ACCEPT,
    PROTO_FOUNDATION_WATCH_CREATE,
    PROTO_FOUNDATION_WATCH_READ,
} ProtoFoundationOp_t;

typedef enum {
    PROTO_WAIT_OBJECT = 1,
    PROTO_WAIT_IPC,
    PROTO_WAIT_SOCKET,
    PROTO_WAIT_JOB,
} ProtoWaitKind_t;

typedef struct {
    uint32_t handle;
    uint16_t kind;
    uint16_t events;
} ProtoWaitItem_t;

typedef struct {
    uint32_t op;
    uint32_t args[6];
    uint32_t buffer;
    uint32_t buffer_offset;
    uint32_t length;
    uint32_t deadline_lo;
    uint32_t deadline_hi;
    uint32_t count;
    ProtoWaitItem_t items[PROTO_WAIT_ITEMS_MAX];
} ProtoFoundationRequest_t;

typedef struct { uint32_t values[8]; } ProtoFoundationResult_t;
typedef struct {
    uint32_t readable;
    uint32_t writable;
    uint32_t error;
    uint32_t closed;
} ProtoWaitResult_t;

/* These are copied once from a registered buffer before authorization. */
typedef struct { char source[96]; char target[96]; } ProtoPathPair_t;
#define PROTO_FILE_INFO_FILE 1u
#define PROTO_FILE_INFO_DIRECTORY 2u
typedef struct {
    uint32_t type, size_lo, size_hi, mtime, rights, open_count;
} ProtoFileInfo_t;
typedef struct {
    uint32_t monotonic_lo, monotonic_hi;
    uint32_t utc_sec_lo, utc_sec_hi;
    uint32_t utc_usec, flags;
} ProtoClockSnapshot_t;

enum {
    PROTO_FEATURE_FILE_LIFECYCLE = 1u << 0,
    PROTO_FEATURE_DURABLE_REPLACE = 1u << 1,
    PROTO_FEATURE_SYNCHRONIZATION = 1u << 2,
    PROTO_FEATURE_WAIT_MANY = 1u << 3,
    PROTO_FEATURE_CLOCK64 = 1u << 4,
    PROTO_FEATURE_SECURE_RANDOM = 1u << 5,
    PROTO_FEATURE_TIMERS = 1u << 6,
    PROTO_FEATURE_JOBS = 1u << 7,
    PROTO_FEATURE_PIPES = 1u << 8,
    PROTO_FEATURE_HANDLE_TRANSFER = 1u << 9,
    PROTO_FEATURE_FILE_WATCH = 1u << 10,
};
/* A zero limit means not reported or not implemented; consult features. */
typedef struct {
    uint32_t size, abi_version, features, flags;
    uint32_t handles_per_process, buffers_per_process, workers_per_process;
    uint32_t wait_items, objects_per_process, waiters_per_process;
    uint32_t allocation_descriptors_per_process, random_bytes, pipe_bytes;
    uint32_t jobs_per_process, transfer_offers_per_process;
    uint32_t reserved;
} ProtoApiLimits_t;
typedef struct { char package[48]; uint32_t flags; } ProtoJobStart_t;
enum {
    PROTO_JOB_PENDING_APPROVAL = 1, PROTO_JOB_QUEUED,
    PROTO_JOB_LAUNCHING, PROTO_JOB_RUNNING, PROTO_JOB_EXITED,
    PROTO_JOB_FAILED,
};
typedef struct {
    uint32_t handle, state, pid, generation;
    int32_t exit_code;
    uint32_t exit_reason;
    int32_t error_code;
} ProtoJobStatus_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(ProtoFileInfo_t) == 24u, "file info size");
_Static_assert(sizeof(ProtoPathPair_t) == 192u, "path pair size");
_Static_assert(sizeof(ProtoFoundationRequest_t) == 116u, "foundation request size");
_Static_assert(_Alignof(ProtoFoundationRequest_t) == 4u, "foundation request alignment");
_Static_assert(sizeof(ProtoWaitItem_t) == 8u, "wait item size");
_Static_assert(sizeof(ProtoClockSnapshot_t) == 24u, "clock snapshot size");
_Static_assert(sizeof(ProtoApiLimits_t) == 64u, "limits size");
#endif
