#pragma once

/*
 * proto_gfx is the retained-mode Game2D helper for ProtoOS applications.
 * It is an SDK library only: every operation below is implemented in terms of
 * the public proto_* ABI and none of its state lives in mutable globals.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "proto_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_GFX_SCREEN_W 320
#define PROTO_GFX_SCREEN_H 240

/*
 * What command_capacity 0 selects: the full raised ceiling.
 *
 * The baseline is generous on purpose. Overrunning the command list is not a
 * graceful failure - proto_gfx latches frame_error on the first overflow and
 * submit then discards the WHOLE frame, so an app that quietly drifts over
 * budget does not lose detail, it loses frames, and if its cost oscillates it
 * loses them in runs and the panel visibly freezes. That is a miserable thing
 * to debug from the symptom, so the default is set where an app has to be
 * genuinely extravagant to reach it.
 *
 * It is close to free. The kernel sizes its per-frame payload from the number
 * of commands actually SUBMITTED, not from the capacity, so an app that draws
 * sixty commands pays for sixty however large its list is. The only real cost
 * is the buffer itself - 32 bytes an entry, out of a PSRAM-backed heap.
 *
 * And it cannot stop an app starting: a DEFAULTED capacity that will not fit
 * the app's heap halves until it does (see proto_gfx_init). An app that names
 * a capacity still gets it or gets an error, because it asked for a reason.
 */
#define PROTO_GFX_DEFAULT_COMMANDS 1024u
/* Floor for that shrink-to-fit. Below this an app cannot draw a useful frame
 * and failing loudly beats limping. */
#define PROTO_GFX_MIN_COMMANDS 128u
/* Bound on sprite_capacity, and on the sort scratch gfx_emit_sprites builds on
 * the stack. That array holds sprite slot indices, so it was only ever related
 * to the command ceiling by accident; pinning it here keeps a bigger command
 * list from costing every Game2D app a kilobyte of stack it cannot use. */
#define PROTO_GFX_MAX_SPRITES 512u
#define PROTO_GFX_DEFAULT_SPRITES 32u
#define PROTO_GFX_DEFAULT_ANIMS   16u
#define PROTO_GFX_DEFAULT_TWEENS  16u
#define PROTO_GFX_DEFAULT_SCENES   8u
#define PROTO_GFX_TILE_EMPTY 0xffffu

typedef uint32_t ProtoGfxSpriteId_t;
#define PROTO_GFX_INVALID_SPRITE 0u

typedef enum {
    PROTO_GFX_FRAME_ACCEPTED = 0,
    PROTO_GFX_FRAME_DROPPED = 1,
    PROTO_GFX_FRAME_IDLE = 2
} ProtoGfxFrameResult_t;

typedef enum {
    PROTO_GFX_TIMING_VARIABLE = 0,
    PROTO_GFX_TIMING_FIXED = 1
} ProtoGfxTimingMode_t;

typedef struct {
    const char *title;
    uint16_t bg;
    uint16_t target_fps;
    uint16_t screen_w;             /* 0 selects 320. */
    uint16_t screen_h;             /* 0 selects 240. */
    uint16_t command_capacity;     /* 0 selects PROTO_GFX_DEFAULT_COMMANDS,
                                    * which shrinks to fit if the heap is
                                    * tight. A named value is exact. */
    uint16_t text_capacity;        /* 0 selects the V2 side-buffer maximum. */
    uint16_t sprite_capacity;      /* 0 selects PROTO_GFX_DEFAULT_SPRITES. */
    uint16_t anim_capacity;        /* 0 selects PROTO_GFX_DEFAULT_ANIMS. */
    uint16_t tween_capacity;       /* 0 selects PROTO_GFX_DEFAULT_TWEENS. */
    uint16_t scene_capacity;       /* 0 selects PROTO_GFX_DEFAULT_SCENES. */
    uint16_t update_hz;            /* Fixed mode: 0 selects target_fps or 60. */
    uint8_t dirty_full_percent;    /* 0 selects 65 percent. */
    uint8_t timing_mode;           /* ProtoGfxTimingMode_t. */
    uint8_t max_catchup_steps;     /* Fixed mode: 0 selects four. */
    uint8_t reserved;
} ProtoGfxConfig_t;

/* Describes one uploaded RGB565 asset or atlas. The kernel owns the asset;
 * this small value only supplies the bounds/source metadata absent from ABI
 * asset handles. */
typedef struct {
    ProtoGameAssetHandle_t handle;
    uint16_t width;
    uint16_t height;
    uint16_t frame_w;
    uint16_t frame_h;
    uint16_t columns;
    uint16_t frame_count;
} ProtoGfxAsset_t;

typedef struct ProtoGfxSpriteSlot ProtoGfxSpriteSlot_t;
typedef struct ProtoGfxScene ProtoGfxScene_t;
typedef struct ProtoGfxAnim ProtoGfxAnim_t;
typedef struct ProtoGfxTween ProtoGfxTween_t;

/* The caller allocates this object (normally with proto_malloc). init owns all
 * pointed-to storage until deinit; copying an initialized ProtoGfx_t is not
 * supported. Fields are public only so sizeof(ProtoGfx_t) is available to C. */
typedef struct ProtoGfx {
    uint32_t magic;
    uint32_t surface_id;
    uint16_t screen_w;
    uint16_t screen_h;
    uint16_t target_fps;
    uint16_t command_capacity;
    uint16_t sprite_capacity;
    uint16_t anim_capacity;
    uint16_t tween_capacity;
    uint16_t scene_capacity;
    uint16_t update_hz;
    uint8_t dirty_full_percent;
    uint8_t frame_open;
    uint8_t full_redraw;
    uint8_t have_presented;
    uint8_t last_frame_accepted;
    uint8_t clip_depth;
    uint8_t timing_mode;
    uint8_t max_catchup_steps;
    uint8_t fixed_steps_due;
    uint8_t layer_depth;

    ProtoGameDrawCommandV2_t *commands;
    char *text;
    ProtoRect_t *dirty_rects;
    ProtoGfxSpriteSlot_t *sprites;
    ProtoGfxAnim_t **anims;
    ProtoGfxTween_t **tweens;
    const ProtoGfxScene_t **scenes;
    ProtoBufferHandle_t command_buffer;
    ProtoBufferHandle_t text_buffer;
    ProtoBufferHandle_t dirty_buffer;

    uint16_t command_count;
    uint16_t text_capacity;
    uint16_t text_bytes;
    uint16_t dirty_count;
    uint16_t scene_count;
    uint32_t next_sprite_sequence;
    uint32_t last_begin_us;
    uint32_t frame_start_us;
    uint32_t dt_carry_us;
    uint32_t dt_ms;
    uint32_t accepted_clock_ms;
    uint32_t frame_count;
    uint32_t accepted_frames;
    uint32_t dropped_frames;
    uint32_t idle_frames;
    uint32_t pace_remainder;
    uint32_t next_frame_deadline_us;
    uint32_t fixed_accumulator_units;
    uint32_t fixed_step_ms_remainder;
    uint16_t fixed_interpolation_q16;
    uint16_t fixed_frame_remainder; /* Step remainder before this frame's steps. */

    ProtoRect_t pending_dirty[PROTO_GAME2D_MAX_DIRTY_RECTS];
    uint8_t pending_dirty_count;
    uint8_t reserved2[3];
    int frame_error;
} ProtoGfx_t;

/* Surface, registered-buffer, and owned-storage lifecycle. */
int proto_gfx_init(ProtoGfx_t *gfx, const ProtoGfxConfig_t *config);
void proto_gfx_deinit(ProtoGfx_t *gfx);
uint32_t proto_gfx_surface(const ProtoGfx_t *gfx);

/* Frame lifecycle. begin resets immediate commands and returns this tick's
 * carry-corrected, 200 ms-clamped dt. submit adds retained sprites in z order,
 * coalesces dirty rectangles, tolerates KERNEL_ERR_TIMEOUT, and paces the
 * caller to target_fps. Negative returns are real errors; ACCEPTED, DROPPED,
 * and IDLE are the non-error results above. */
uint32_t proto_gfx_begin_frame(ProtoGfx_t *gfx);
int proto_gfx_submit(ProtoGfx_t *gfx);
int proto_gfx_run_frame(ProtoGfx_t *gfx);
uint32_t proto_gfx_dt_ms(const ProtoGfx_t *gfx);
uint8_t proto_gfx_update_steps(const ProtoGfx_t *gfx);
uint32_t proto_gfx_fixed_step_us(const ProtoGfx_t *gfx);
uint16_t proto_gfx_interpolation_q16(const ProtoGfx_t *gfx);
uint32_t proto_gfx_accepted_clock_ms(const ProtoGfx_t *gfx);
bool proto_gfx_last_frame_accepted(const ProtoGfx_t *gfx);
int proto_gfx_set_target_fps(ProtoGfx_t *gfx, uint16_t target_fps);
void proto_gfx_invalidate_rect(ProtoGfx_t *gfx, ProtoRect_t rect);
void proto_gfx_invalidate_all(ProtoGfx_t *gfx);

/* Atlas and retained sprites. */
int proto_gfx_asset_init(ProtoGfxAsset_t *asset,
                         ProtoGameAssetHandle_t handle,
                         uint16_t width, uint16_t height);
int proto_gfx_asset_atlas_init(ProtoGfxAsset_t *asset,
                               ProtoGameAssetHandle_t handle,
                               uint16_t width, uint16_t height,
                               uint16_t frame_w, uint16_t frame_h);
ProtoGfxSpriteId_t proto_gfx_sprite_add(ProtoGfx_t *gfx,
                                        const ProtoGfxAsset_t *asset,
                                        int16_t x, int16_t y, int16_t z);
int proto_gfx_sprite_move(ProtoGfx_t *gfx, ProtoGfxSpriteId_t sprite,
                          int16_t x, int16_t y);
int proto_gfx_sprite_set_frame(ProtoGfx_t *gfx,
                               ProtoGfxSpriteId_t sprite,
                               uint16_t frame);
int proto_gfx_sprite_set_visible(ProtoGfx_t *gfx,
                                 ProtoGfxSpriteId_t sprite, bool visible);
int proto_gfx_sprite_set_z(ProtoGfx_t *gfx, ProtoGfxSpriteId_t sprite,
                           int16_t z);
int proto_gfx_sprite_set_scale(ProtoGfx_t *gfx, ProtoGfxSpriteId_t sprite,
                               uint8_t scale);
int proto_gfx_sprite_set_flip(ProtoGfx_t *gfx, ProtoGfxSpriteId_t sprite,
                              bool flip_x, bool flip_y);
int proto_gfx_sprite_set_opacity(ProtoGfx_t *gfx,
                                 ProtoGfxSpriteId_t sprite,
                                 uint8_t opacity);
int proto_gfx_sprite_set_palette(ProtoGfx_t *gfx,
                                 ProtoGfxSpriteId_t sprite,
                                 ProtoGamePaletteHandle_t palette);
int proto_gfx_sprite_remove(ProtoGfx_t *gfx, ProtoGfxSpriteId_t sprite);
int proto_gfx_sprite_bounds(const ProtoGfx_t *gfx,
                            ProtoGfxSpriteId_t sprite, ProtoRect_t *out_bounds);
int proto_gfx_sprite_set_collision_mask(ProtoGfx_t *gfx,
                                         ProtoGfxSpriteId_t sprite,
                                         uint32_t mask);
int proto_gfx_sprite_set_tag(ProtoGfx_t *gfx, ProtoGfxSpriteId_t sprite,
                              uint32_t tag);
uint32_t proto_gfx_sprite_tag(const ProtoGfx_t *gfx,
                              ProtoGfxSpriteId_t sprite);

/* Immediate draw commands. They are rebuilt after every begin_frame and are
 * emitted before retained sprites. Moving immediate-mode content must be
 * paired with invalidate_rect/all; retained sprites do this automatically. */
int proto_gfx_fill_rect(ProtoGfx_t *gfx, int16_t x, int16_t y,
                        int16_t w, int16_t h, uint16_t color);
int proto_gfx_fill_rect_ex(ProtoGfx_t *gfx, int16_t x, int16_t y,
                           int16_t w, int16_t h, uint16_t color,
                           uint8_t opacity);
int proto_gfx_text(ProtoGfx_t *gfx, int16_t x, int16_t y,
                   uint16_t color, const char *text);
int proto_gfx_text_ex(ProtoGfx_t *gfx, int16_t x, int16_t y,
                      uint16_t color, ProtoGameFont_t font, uint8_t scale,
                      uint8_t opacity, const char *text);
int proto_gfx_sprite_draw(ProtoGfx_t *gfx, const ProtoGfxAsset_t *asset,
                          uint16_t frame, int16_t x, int16_t y);
int proto_gfx_sprite_draw_ex(ProtoGfx_t *gfx,
                             const ProtoGfxAsset_t *asset,
                             uint16_t frame, int16_t x, int16_t y,
                             uint8_t scale, bool flip_x, bool flip_y,
                             ProtoGamePaletteHandle_t palette,
                             uint8_t opacity);
/* Draws one source rectangle without requiring a uniform sprite-sheet grid. */
int proto_gfx_sprite_draw_region_ex(ProtoGfx_t *gfx,
                                    const ProtoGfxAsset_t *asset,
                                    uint16_t src_x, uint16_t src_y,
                                    uint16_t src_w, uint16_t src_h,
                                    int16_t x, int16_t y,
                                    uint8_t scale, bool flip_x, bool flip_y,
                                    ProtoGamePaletteHandle_t palette,
                                    uint8_t opacity);
/* Draws a complete A8 coverage asset, scaled to the destination rectangle.
 * The framebuffer remains RGB565; color is the tint and opacity multiplies
 * the per-pixel coverage. */
int proto_gfx_mask_draw(ProtoGfx_t *gfx, const ProtoGfxAsset_t *mask,
                        int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t color, uint8_t opacity);
/* Draws and scales one rectangle from an A8 atlas. This is the preferred
 * primitive for glyph and icon sheets; it does not allocate or upload a
 * temporary asset. */
int proto_gfx_mask_draw_region(ProtoGfx_t *gfx,
                               const ProtoGfxAsset_t *mask,
                               uint16_t src_x, uint16_t src_y,
                               uint16_t src_w, uint16_t src_h,
                               int16_t x, int16_t y,
                               int16_t w, int16_t h,
                               uint16_t color, uint8_t opacity);
/* Bracket destination-independent chrome. The kernel caches the resulting
 * translucent layer by cache_id/revision and composites it over the live
 * background. Change revision whenever any enclosed visual changes. Moving
 * x/y alone translates a cached layer; enclosed coordinates must move by the
 * same delta. Layers cannot nest and must begin outside a clip stack. */
int proto_gfx_layer_begin(ProtoGfx_t *gfx, uint32_t cache_id,
                          uint32_t revision, int16_t x, int16_t y,
                          int16_t w, int16_t h);
int proto_gfx_layer_end(ProtoGfx_t *gfx);
int proto_gfx_clip_push(ProtoGfx_t *gfx, int16_t x, int16_t y,
                        int16_t w, int16_t h);
int proto_gfx_clip_pop(ProtoGfx_t *gfx);
int proto_gfx_ellipse(ProtoGfx_t *gfx, int16_t cx, int16_t cy,
                      int16_t rx, int16_t ry, int16_t step, uint16_t color);
/* outline_width 0 fills the shape; positive values draw an inset outline. */
int proto_gfx_ellipse_ex(ProtoGfx_t *gfx, int16_t cx, int16_t cy,
                         int16_t rx, int16_t ry, int16_t outline_width,
                         uint16_t color, uint8_t opacity);
int proto_gfx_circle(ProtoGfx_t *gfx, int16_t cx, int16_t cy,
                     int16_t radius, uint16_t color);
int proto_gfx_circle_ex(ProtoGfx_t *gfx, int16_t cx, int16_t cy,
                        int16_t radius, int16_t outline_width,
                        uint16_t color, uint8_t opacity);
int proto_gfx_line(ProtoGfx_t *gfx, int16_t x0, int16_t y0,
                   int16_t x1, int16_t y1, int16_t thickness,
                   uint16_t color);
int proto_gfx_line_ex(ProtoGfx_t *gfx, int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1, int16_t thickness,
                      uint16_t color, uint8_t opacity);
int proto_gfx_round_rect(ProtoGfx_t *gfx, int16_t x, int16_t y,
                         int16_t w, int16_t h, int16_t radius,
                         uint16_t color);
int proto_gfx_round_rect_ex(ProtoGfx_t *gfx, int16_t x, int16_t y,
                            int16_t w, int16_t h, int16_t radius,
                            int16_t outline_width, uint16_t color,
                            uint8_t opacity);
int proto_gfx_outline(ProtoGfx_t *gfx, int16_t x, int16_t y,
                      int16_t w, int16_t h, int16_t thickness,
                      uint16_t color);

/* Tilemap cells are zero-based atlas frames; PROTO_GFX_TILE_EMPTY is skipped. */
typedef struct {
    const ProtoGfxAsset_t *atlas;
    const uint16_t *cells;
    uint16_t columns;
    uint16_t rows;
    uint16_t tile_w;
    uint16_t tile_h;
} ProtoGfxTilemap_t;

int proto_gfx_tilemap_draw(ProtoGfx_t *gfx,
                           const ProtoGfxTilemap_t *tilemap,
                           int32_t camera_x, int32_t camera_y);

/* AABB collision against current, visible retained sprites. Query returns the
 * total number of matches even if out_sprites is smaller; mask 0 matches all. */
bool proto_gfx_aabb_overlap(ProtoRect_t a, ProtoRect_t b);
size_t proto_gfx_query_aabb(const ProtoGfx_t *gfx, ProtoRect_t bounds,
                            uint32_t mask, ProtoGfxSpriteId_t ignore,
                            ProtoGfxSpriteId_t *out_sprites, size_t out_cap);

typedef enum {
    PROTO_GFX_ANIM_LOOP = 0,
    PROTO_GFX_ANIM_PINGPONG,
    PROTO_GFX_ANIM_ONCE
} ProtoGfxAnimMode_t;

struct ProtoGfxAnim {
    ProtoGfxSpriteId_t sprite;
    const uint16_t *frames;
    uint16_t frame_count;
    uint16_t cursor;
    uint32_t ms_per_frame;
    uint32_t elapsed_ms;
    uint8_t mode;
    int8_t direction;
    uint8_t active;
    uint8_t registered;
};

int proto_gfx_anim_start(ProtoGfx_t *gfx, ProtoGfxAnim_t *anim,
                         ProtoGfxSpriteId_t sprite,
                         const uint16_t *frames, uint16_t frame_count,
                         uint32_t ms_per_frame, ProtoGfxAnimMode_t mode);
void proto_gfx_anim_stop(ProtoGfx_t *gfx, ProtoGfxAnim_t *anim);

typedef enum {
    PROTO_GFX_EASE_LINEAR = 0,
    PROTO_GFX_EASE_QUAD_IN,
    PROTO_GFX_EASE_QUAD_OUT,
    PROTO_GFX_EASE_QUAD_IN_OUT,
    PROTO_GFX_EASE_CUBIC_IN,
    PROTO_GFX_EASE_CUBIC_OUT,
    PROTO_GFX_EASE_CUBIC_IN_OUT,
    PROTO_GFX_EASE_BACK_OUT,
    PROTO_GFX_EASE_ELASTIC_OUT
} ProtoGfxEase_t;

typedef void (*ProtoGfxTweenDoneFn)(ProtoGfx_t *gfx,
                                     ProtoGfxTween_t *tween, void *user);

struct ProtoGfxTween {
    int32_t *target;
    int32_t from;
    int32_t to;
    uint32_t duration_ms;
    uint32_t elapsed_ms;
    uint8_t easing;
    uint8_t active;
    uint8_t registered;
    uint8_t reserved;
    ProtoGfxTweenDoneFn on_done;
    void *user;
};

int proto_gfx_tween_start(ProtoGfx_t *gfx, ProtoGfxTween_t *tween,
                          int32_t *target, int32_t from, int32_t to,
                          uint32_t duration_ms, ProtoGfxEase_t easing,
                          ProtoGfxTweenDoneFn on_done, void *user);
void proto_gfx_tween_cancel(ProtoGfx_t *gfx, ProtoGfxTween_t *tween);
int32_t proto_gfx_ease(ProtoGfxEase_t easing, int32_t from, int32_t to,
                       uint16_t progress_q16);

typedef struct {
    uint32_t type;
    int32_t value;
    uint32_t timestamp_ms;
    const void *data;
    size_t data_len;
} ProtoGfxInput_t;

typedef void (*ProtoGfxSceneEnterFn)(ProtoGfx_t *gfx,
                                      const ProtoGfxScene_t *scene);
typedef void (*ProtoGfxSceneTickFn)(ProtoGfx_t *gfx,
                                    const ProtoGfxScene_t *scene,
                                    uint32_t dt_ms);
typedef void (*ProtoGfxSceneDrawFn)(ProtoGfx_t *gfx,
                                    const ProtoGfxScene_t *scene);
typedef bool (*ProtoGfxSceneInputFn)(ProtoGfx_t *gfx,
                                     const ProtoGfxScene_t *scene,
                                     const ProtoGfxInput_t *input);

struct ProtoGfxScene {
    ProtoGfxSceneEnterFn on_enter;
    ProtoGfxSceneTickFn on_tick;
    ProtoGfxSceneDrawFn on_draw;
    ProtoGfxSceneInputFn on_input;
    void *user;
};

int proto_gfx_scene_push(ProtoGfx_t *gfx, const ProtoGfxScene_t *scene);
const ProtoGfxScene_t *proto_gfx_scene_pop(ProtoGfx_t *gfx);
const ProtoGfxScene_t *proto_gfx_scene_top(const ProtoGfx_t *gfx);
void proto_gfx_scene_tick(ProtoGfx_t *gfx);
void proto_gfx_scene_draw(ProtoGfx_t *gfx);
bool proto_gfx_scene_input(ProtoGfx_t *gfx, const ProtoGfxInput_t *input);

#ifdef __cplusplus
}
#endif
