#pragma once
#include <stdint.h>

/* Shared fixed-width Game2D command wire records, also used by Game3D HUDs. */
typedef enum {
    PROTO_GAME2D_CMD_V2_CLEAR = 0,
    PROTO_GAME2D_CMD_V2_FILL_RECT,
    PROTO_GAME2D_CMD_V2_SPRITE,
    PROTO_GAME2D_CMD_V2_TEXT,
    PROTO_GAME2D_CMD_V2_CLIP_PUSH,
    PROTO_GAME2D_CMD_V2_CLIP_POP,
    /* Scale an A8 source rectangle to w x h and tint it with color. src_x/y
     * select its origin. text_offset/text_length carry its width/height for
     * this op only; zero width and height retain the legacy whole/remainder
     * asset behavior. */
    PROTO_GAME2D_CMD_V2_MASK,
    /* Cache a destination-independent premultiplied-alpha layer. `asset` is
     * the app-selected cache key and `color` is its content revision; x/y/w/h
     * are a hard clip and allocation bound. Commands remain in the submitted
     * snapshot, so a cache miss can always render them normally. */
    PROTO_GAME2D_CMD_V2_LAYER_BEGIN,
    PROTO_GAME2D_CMD_V2_LAYER_END,
    /* Analytic antialiased primitive. `font` selects ProtoGameShape_t.
     * Rounded boxes and ellipses use x/y/w/h as their bounds and src_y as
     * outline width (zero fills); rounded boxes use src_x as radius. Lines
     * use x/y and w/h as endpoints and src_x as thickness. */
    PROTO_GAME2D_CMD_V2_SMOOTH_SHAPE
} ProtoGameDrawOpV2_t;

typedef enum {
    PROTO_GAME2D_SHAPE_ROUND_RECT = 0u,
    PROTO_GAME2D_SHAPE_ELLIPSE = 1u,
    PROTO_GAME2D_SHAPE_LINE = 2u
} ProtoGameShape_t;

#define PROTO_GAME2D_CMD_FLAG_FLIP_X       0x01u
#define PROTO_GAME2D_CMD_FLAG_FLIP_Y       0x02u
#define PROTO_GAME2D_CMD_FLAG_SCALE_MASK   0x0cu
#define PROTO_GAME2D_CMD_FLAG_SCALE_1X     0x00u
#define PROTO_GAME2D_CMD_FLAG_SCALE_2X     0x04u
#define PROTO_GAME2D_CMD_FLAG_SCALE_3X     0x08u

typedef enum {
    PROTO_GAME2D_FONT_FIXED_8 = 0u,
    PROTO_GAME2D_FONT_PROP_8 = 1u
} ProtoGameFont_t;

typedef struct {
    uint8_t op;
    uint8_t flags;
    uint8_t opacity; /* 0 transparent, 255 opaque. */
    uint8_t font;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint32_t color;
    uint32_t asset;
    uint32_t palette; /* 0 uses the asset's default LUT. */
    uint16_t src_x;
    uint16_t src_y;
    uint16_t text_offset;
    uint16_t text_length;
} ProtoGameDrawCommandV2_t;
