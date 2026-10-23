#include "proto_loaded.h"
#include "proto_gfx.h"

int proto_loaded_main(const ProtoElfLoadMap_t *load_map,
                      const ProtoElfStartInfo_t *start_info) {
    (void)load_map;
    (void)start_info;
    ProtoGfx_t *gfx = proto_malloc(sizeof(*gfx));
    if (gfx == NULL) return -1;
    const ProtoGfxConfig_t config = {
        .title = "SDK Hello 2D", .bg = 0x0841, .target_fps = 30,
        .command_capacity = 16, .text_capacity = 64,
        .sprite_capacity = 1, .anim_capacity = 1,
        .tween_capacity = 1, .scene_capacity = 1,
    };
    int rc = proto_gfx_init(gfx, &config);
    if (rc != KERNEL_OK) {
        proto_log(LOG_ERROR, "FAIL SDK hello2d: surface initialization refused");
        proto_free(gfx);
        return rc;
    }
    (void)proto_gfx_begin_frame(gfx);
    rc = proto_gfx_fill_rect(gfx, 24, 64, 272, 112, 0x041f);
    if (rc == KERNEL_OK)
        rc = proto_gfx_text(gfx, 48, 108, 0xffff, "RISK4 / ProtoOS SDK");
    if (rc == KERNEL_OK) rc = proto_gfx_submit(gfx);
    if (rc == PROTO_GFX_FRAME_ACCEPTED) {
        proto_log(LOG_WARN, "PASS SDK hello2d: frame accepted (presentation unverified)");
        proto_sleep_ms(2000);
    } else {
        proto_log(LOG_WARN, "FAIL SDK hello2d: frame not accepted; retry after closing other apps");
    }
    proto_gfx_deinit(gfx);
    proto_free(gfx);
    return rc == PROTO_GFX_FRAME_ACCEPTED ? 0 : -1;
}
