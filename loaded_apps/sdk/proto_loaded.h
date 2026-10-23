#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "../../main/include/proto_abi.h"

/*
 * Build-tool metadata. The macro compiles away, but tools/protelf_dev.py reads
 * the string when auto-generating manifest capabilities and flags.
 *
 * Example:
 *   PROTO_APP_METADATA("package=dev.proto.myapp hidden heap=8192")
 */
#ifndef PROTO_APP_METADATA
#define PROTO_APP_METADATA(...)
#endif

/* Memory */
void *memset(void *dst, int value, size_t len);
void *memcpy(void *dst, const void *src, size_t len);
void *memmove(void *dst, const void *src, size_t len);
int   memcmp(const void *a, const void *b, size_t len);

/* String */
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strchr(const char *s, int c);

/* Formatting — use instead of sprintf/printf */
int proto_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);
int proto_snprintf(char *buf, size_t cap, const char *fmt, ...);

typedef struct ProtoLoadedContext ProtoLoadedContext;

ProtoLoadedContext *proto_loaded_self(void);
uint32_t proto_pid(void);
void proto_log(LogLevel_t level, const char *msg);
int proto_logf(LogLevel_t level, const char *fmt, ...);
int proto_send(uint32_t target_pid, uint32_t msg_type,
               const void *payload, size_t payload_len);
bool proto_recv(IpcMessage_t *out_msg, uint32_t timeout_ms);
void proto_wait_ready(void);
void proto_sleep_ms(uint32_t ms);
/* Absolute modulo-2^32 proto_time_us() deadline. */
void proto_sleep_until_us(uint32_t deadline_us);
/* Cooperative cancellation/fairness point for long compute loops. */
void proto_thread_yield(void);
// Monotonic microseconds from the kernel clock. This remains correct when a
// dynamically scheduled worker migrates between the P4's per-hart counters.
uint32_t proto_time_us(void);
// Always enters the kernel through the full syscall security path.
uint32_t proto_time_us_syscall(void);
/* Starts a bounded worker owned by this loaded process. Each U-mode worker
 * receives a separate sealed user stack and may run on either ESP32-P4 core
 * according to core_policy. The entry receives config.argument; SDK context
 * is already installed in tp, so a worker calls normal proto_* APIs directly.
 * Returning is the supported worker-exit path. argument must reference
 * process-shared user memory, not another U-mode thread's private stack.
 * config.stack_bytes sizes this worker's private U-mode stack and is rounded
 * to the kernel's PMP allocation class. */
int proto_thread_create(const ProtoThreadConfig_t *config,
                        ProtoThreadHandle_t *out_thread);
/* Polls the kernel's nonblocking JOIN primitive. timeout_ms may be zero or
 * PROTO_THREAD_WAIT_FOREVER. Success consumes the handle permanently. */
int proto_thread_join(ProtoThreadHandle_t thread, uint32_t timeout_ms);
/* Gives up join ownership. Kernel resources are reclaimed after return. */
int proto_thread_detach(ProtoThreadHandle_t thread);
void *proto_malloc(size_t size);
void proto_free(void *ptr);
int proto_open(const char *path, uint32_t flags, ProtoHandle_t *out_handle);
int proto_close(ProtoHandle_t handle);
int proto_read(ProtoHandle_t handle, void *buffer, size_t len, size_t *out_len);
int proto_write(ProtoHandle_t handle, const void *buffer, size_t len, size_t *out_len);
int proto_resource_open(const char *name, ProtoResourceHandle_t *out_resource,
                        ProtoResourceInfo_t *out_info);
int proto_resource_read(ProtoResourceHandle_t resource, uint32_t chunk_index,
                        ProtoBufferHandle_t buffer, uint32_t buffer_offset,
                        ProtoResourceChunkInfo_t *out_info);
int proto_resource_close(ProtoResourceHandle_t resource);
/* Decode one authenticated resource chunk returned by proto_resource_read().
 * stored may alias neither output nor its active range. */
int proto_resource_decode_chunk(const void *stored,
                                const ProtoResourceChunkInfo_t *info,
                                void *output, size_t output_size);
int proto_buffer_register(void *buffer, size_t len, uint32_t flags,
                          ProtoBufferHandle_t *out_buffer);
int proto_buffer_unregister(ProtoBufferHandle_t buffer);
int proto_read_buf(ProtoHandle_t handle, ProtoBufferHandle_t buffer,
                   size_t offset, size_t len, size_t *out_len);
int proto_write_buf(ProtoHandle_t handle, ProtoBufferHandle_t buffer,
                    size_t offset, size_t len, size_t *out_len);
int proto_ioctl(ProtoHandle_t handle, uint32_t op, const ProtoIoctlRequest_t *request);
int proto_file_rewind_truncate(ProtoHandle_t handle);
/* Kernel-mixed signed 16-bit PCM. Open one long-lived stream, feed it from a
 * paced worker, and join that worker before closing the handle or its buffers.
 * volume_pct is per-stream gain; the Settings master volume is applied later. */
int proto_audio_open(uint32_t sample_rate, uint32_t channels,
                     uint32_t volume_pct, ProtoHandle_t *out_handle);
int proto_audio_write(ProtoHandle_t handle, const void *pcm, size_t len,
                      size_t *out_len);
int proto_audio_set_volume(ProtoHandle_t handle, uint32_t volume_pct);
int proto_audio_tone(ProtoHandle_t handle, uint32_t frequency_hz,
                     uint32_t duration_ms, uint32_t volume_pct);
/* Drops queued PCM and tones immediately; this is not a drain operation. */
int proto_audio_flush(ProtoHandle_t handle);
int proto_audio_stat(ProtoHandle_t handle, ProtoAudioStat_t *out_stat);
int proto_file_dialog_open(uint32_t requested_rights, const char *suffix_filter,
                           ProtoHandle_t *out_handle, char *out_path, size_t out_path_size);
int proto_file_dialog_save(const char *suggested_name, const char *suffix_filter,
                           ProtoHandle_t *out_handle, char *out_path, size_t out_path_size);
int proto_user_verify(ProtoAuthPurpose_t purpose, const char *reason);
int proto_user_verify_app(ProtoAuthPurpose_t purpose, uint32_t target_app_id,
                          const char *reason);
int proto_permission_request(uint32_t permission,
                             ProtoPermissionGrant_t grant,
                             const char *reason);
int proto_system_action_request(ProtoSystemAction_t action,
                                const char *reason);
int proto_ui_background_request(const char *reason);
int proto_ui_exit_request(uint32_t reason, uint32_t flags);
/* ABI 20: held/pressed USB keyboard keys (HID usage bitmaps). Bits are live
 * only while PROTO_KEY_STATE_FOCUSED is set. Poll once per frame. */
int proto_key_state(ProtoKeyState_t *out_state);
/* ABI 25: BLE mouse motion and buttons since this app's previous read
 * (foreground only). Older kernels reject the unknown syscall. */
int proto_pointer_state(ProtoPointerState_t *out_state);
/* ABI 26: BLE gamepad sticks, triggers and buttons (foreground only). Check
 * proto_api_limits().abi_version >= 26 first: older kernels reject it. */
int proto_gamepad_state(ProtoGamepadState_t *out_state);
/* ABI 22: the system theme (colour roles, shape, badge). Public, no
 * capability. In UI commands, PROTO_UI_ROLE(PROTO_THEME_ROLE_*) as a colour
 * follows the theme without calling this at all. */
int proto_theme_get(ProtoTheme_t *out_theme);

/* ABI 24: namespace-authorized file lifecycle. Discovery reports only
 * implemented features; durable replacement and waits remain unavailable. */
int proto_rename(const char *source, const char *target, uint32_t flags);
int proto_rmdir(const char *path);
int proto_fstat(ProtoHandle_t handle, ProtoFileInfo_t *out_info);
int proto_ftruncate(ProtoHandle_t handle, uint64_t length);
int proto_fsync(ProtoHandle_t handle);
int proto_api_limits(ProtoApiLimits_t *out_limits);

int proto_stat(const char *path, ProtoStat_t *out_stat);
int proto_mkdir(const char *path);
int proto_unlink(const char *path);
int proto_listdir(const char *path, ProtoDirEntry_t *entries,
                  size_t entry_cap, size_t *out_count);
int proto_shm_open(size_t size, uint32_t flags,
                   ProtoHandle_t *out_handle, ProtoShmInfo_t *out_info);
int proto_shm_map(ProtoHandle_t handle, ProtoShmInfo_t *out_info);
int proto_ui_open_surface(const char *title, uint32_t *out_surface_id);
int proto_ui_submit_commands(uint32_t surface_id,
                             ProtoBufferHandle_t command_buffer,
                             uint32_t command_count);
int proto_ui_scroll(uint32_t surface_id, int16_t y, bool animated);
int proto_ui_close_surface(uint32_t surface_id);
int proto_game_open_surface(const char *title, uint16_t bg_color,
                            uint32_t *out_surface_id);
int proto_game_query_capabilities(ProtoGameCapabilities_t *out_caps);
int proto_game_upload_asset(uint32_t surface_id,
                            ProtoBufferHandle_t pixel_buffer,
                            const ProtoGameAssetDesc_t *desc,
                            ProtoGameAssetHandle_t *out_asset);
int proto_game_upload_palette(uint32_t surface_id,
                              ProtoBufferHandle_t color_buffer,
                              uint16_t color_count,
                              ProtoGamePaletteHandle_t *out_palette);
int proto_game_submit_frame(uint32_t surface_id,
                            ProtoBufferHandle_t draw_buffer,
                            uint32_t draw_count,
                            ProtoBufferHandle_t dirty_buffer,
                            uint32_t dirty_count,
                            uint32_t frame_flags);
int proto_game_submit_frame_v2(uint32_t surface_id,
                               ProtoBufferHandle_t draw_buffer,
                               uint32_t draw_count,
                               ProtoBufferHandle_t text_buffer,
                               uint32_t text_bytes,
                               ProtoBufferHandle_t dirty_buffer,
                               uint32_t dirty_count,
                               uint32_t frame_flags);
int proto_game_destroy_asset(uint32_t surface_id,
                             ProtoGameAssetHandle_t asset);
int proto_game_destroy_palette(uint32_t surface_id,
                               ProtoGamePaletteHandle_t palette);
int proto_net_status(ProtoNetStatus_t *out_status);
int proto_net_tcp_connect(const char *host, uint16_t port, uint16_t timeout_ms,
                          ProtoHandle_t *out_handle);
int proto_net_tcp_read(ProtoHandle_t handle, ProtoBufferHandle_t buffer,
                       size_t offset, size_t len, size_t *out_len);
int proto_net_tcp_read_timeout(ProtoHandle_t handle, ProtoBufferHandle_t buffer,
                               size_t offset, size_t len, uint16_t timeout_ms,
                               size_t *out_len);
int proto_net_tcp_write(ProtoHandle_t handle, ProtoBufferHandle_t buffer,
                        size_t offset, size_t len, size_t *out_len);
int proto_net_tcp_write_timeout(ProtoHandle_t handle, ProtoBufferHandle_t buffer,
                                size_t offset, size_t len, uint16_t timeout_ms,
                                size_t *out_len);
int proto_net_tcp_listen(uint16_t port, uint16_t backlog,
                         ProtoHandle_t *out_listener,
                         uint16_t *out_bound_port);
int proto_net_tcp_accept(ProtoHandle_t listener, uint16_t timeout_ms,
                         ProtoNetAcceptResult_t *out_result);
int proto_net_poll(ProtoBufferHandle_t items, size_t offset, uint32_t count,
                   uint16_t timeout_ms, size_t *out_ready);
int proto_net_tcp_shutdown(ProtoHandle_t handle, uint16_t direction);
int proto_net_tcp_close(ProtoHandle_t handle);
ProtoDlHandle_t proto_dlopen(const char *soname, uint32_t flags);
void *proto_dlsym(ProtoDlHandle_t handle, const char *symbol);
int proto_dlclose(ProtoDlHandle_t handle);
/* Loader operations are serialized across workers. Errors are thread-local
 * and consumed by proto_dlerror(); a second call returns NULL until that
 * thread records another loader failure. dlopen/dlclose from an init/fini
 * callback are rejected to keep constructor transitions transactional. */
const char *proto_dlerror(void);
void proto_exit(void);

int proto_loaded_main(const ProtoElfLoadMap_t *load_map,
                      const ProtoElfStartInfo_t *start_info);

int proto_3d_exchange(ProtoBufferHandle_t control,uint32_t offset);
