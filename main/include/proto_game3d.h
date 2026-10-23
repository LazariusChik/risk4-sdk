#pragma once
#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include "proto_game2d_commands.h"

/* Experimental Game3D protocol 1. PROTO_GRAPHICS_GAME3D carries a registered
 * control record through the graphics gateway under KERNEL_CAP_GAME2D.
 * Little-endian integers and IEEE binary32 floats, naturally aligned to 4.
 * No pointers, native enums, bools, size_t or compiler packing on the wire.
 * Reserved fields/flags and unused instance bindings must be zero. */
#define PROTO_3D_VERSION 1u
#define PROTO_3D_MAX_INSTANCES 64u
/* Per mesh. */
#define PROTO_3D_MAX_POSITIONS 1024u
#define PROTO_3D_MAX_UVS 1024u
#define PROTO_3D_MAX_INPUT_TRIANGLES 1024u
/* Per frame, summed over its draws (Proto3DCaps_t reports the running
 * kernel's values; kernels before 2026-10-18 allowed 1,024 of each). */
#define PROTO_3D_FRAME_POSITIONS 3072u
#define PROTO_3D_FRAME_TRIANGLES 3072u
#define PROTO_3D_MAX_CLIPPED_TRIANGLES 3072u
#define PROTO_3D_MAX_TILE_REFS 32768u
#define PROTO_3D_MAX_BINDINGS 64u
#define PROTO_3D_MAX_RESOURCES 64u
#define PROTO_3D_MAX_TEXTURE_SIDE 256u
#define PROTO_3D_RESOURCE_BYTES (512u*1024u)
#define PROTO_3D_TRANSIENT_BYTES (1024u*1024u)
#define PROTO_3D_MAX_CANDIDATE_PIXELS (2048u*1024u)
#define PROTO_3D_COMPLETION_HISTORY 8u
#define PROTO_3D_MAX_LIGHTS 8u
/* Longest single SUBMIT wait (PROTO_3D_REQUEST_WAIT); an app loop that wants
 * to keep waiting submits again, so input and exit stay responsive. */
#define PROTO_3D_MAX_SUBMIT_WAIT_MS 100u

enum {
    PROTO_3D_QUERY_CAPS=0, PROTO_3D_OPEN=1, PROTO_3D_CREATE_MESH=2,
    PROTO_3D_CREATE_TEXTURE=3, PROTO_3D_CREATE_MATERIAL=4, PROTO_3D_DESTROY=5,
    PROTO_3D_SUBMIT=6, PROTO_3D_QUERY_FRAME=7, PROTO_3D_CLOSE=8
};
enum {
    PROTO_3D_OK=0, PROTO_3D_INVALID=-1, PROTO_3D_NOT_FOUND=-2,
    PROTO_3D_BUSY=-5, PROTO_3D_DENIED=-10, PROTO_3D_NOT_SUPPORTED=-15,
    PROTO_3D_DRIVER=-20, PROTO_3D_NO_SPACE=-21, PROTO_3D_CANCELLED=-30
};
enum {
    PROTO_3D_FRAME_UNKNOWN=0, PROTO_3D_FRAME_QUEUED=1,
    PROTO_3D_FRAME_EXECUTING=2, PROTO_3D_FRAME_COMPLETE=3,
    PROTO_3D_FRAME_PRESENTED=4, PROTO_3D_FRAME_CANCELLED=5,
    PROTO_3D_FRAME_REJECTED=6
};
enum {
    PROTO_3D_CAP_OPAQUE=1u, PROTO_3D_CAP_RGB565=2u, PROTO_3D_CAP_REPEAT=4u,
    PROTO_3D_CAP_PERSPECTIVE=8u, PROTO_3D_CAP_FLAT=16u,
    PROTO_3D_CAP_CONSTANT_LIGHT=32u, PROTO_3D_CAP_INSTANCING=64u,
    /* Additions within protocol 1, carried in fields older kernels require to
     * be zero (so they reject them as INVALID): check caps before use. */
    PROTO_3D_CAP_LIGHTING=128u,  /* LIT materials, mesh normals, frame lights */
    PROTO_3D_CAP_CULL_BACK=256u, /* PROTO_3D_MATERIAL_CULL_BACK */
    PROTO_3D_CAP_DITHERED_ALPHA=512u, /* material opacity, fixed 4x4 screen-door mask */
    PROTO_3D_CAP_BLEND=1024u, /* BLEND_ALPHA/ADD/MULTIPLY materials, sorted back to front */
    PROTO_3D_CAP_FOG=2048u,   /* PROTO_3D_FRAME_FOG linear depth fog, NO_FOG materials */
    PROTO_3D_CAP_VERTEX_COLOR=4096u, /* optional per-position RGB8, material modulation */
    /* Orthographic cameras (clip w constant 1; see proto_3d_orthographic)
     * and PROTO_3D_FOG_DEPTH, the fog source that varies under them. */
    PROTO_3D_CAP_ORTHOGRAPHIC=8192u,
    PROTO_3D_CAP_CLAMP=16384u,
    PROTO_3D_CAP_INDEX8=32768u,
    PROTO_3D_CAP_HUD=65536u,
    PROTO_3D_CAP_SUBMIT_WAIT=131072u, /* PROTO_3D_REQUEST_WAIT on SUBMIT */
    PROTO_3D_CAP_COVERAGE=262144u, /* per-instance screen-door masks; FRAME_COVERAGE */
    PROTO_3D_CAP_MIPMAPS=524288u, /* complete offline texture chains; nearest mip */
    PROTO_3D_CAP_SCALE=1048576u /* PROTO_3D_FRAME_SCALE_*: performance-mode render scales */
};
#define PROTO_3D_TEXTURE_RGB565 1u
/* Header, 256 native RGB565 palette entries, then width*height indices.
 * Palette bytes are included in resource accounting; no RGB565 expansion. */
#define PROTO_3D_TEXTURE_INDEX8 2u
/* Proto3DMaterial_t.flags */
enum {
    /* Skip triangles facing away (counter-clockwise front faces). Only for
     * closed or one-sided surfaces the camera never sees from behind. */
    PROTO_3D_MATERIAL_CULL_BACK=1u,
    /* Gouraud-lit by the frame's lights; light becomes a 0..255 strength. */
    PROTO_3D_MATERIAL_LIT=2u,
    /* Opacity is rounded to 0..16 covered samples in each screen-aligned 4x4
     * Bayer cell. Discarded samples write neither color nor depth. Kept ones
     * use ordinary LESS depth and write both. No sorting or color blending. */
    PROTO_3D_MATERIAL_DITHERED_ALPHA=4u,
    /* At most one of DITHERED_ALPHA and the BLEND modes. Blended samples test
     * depth LEQUAL (a second pass over the same mesh and transform draws;
     * decals built from other triangles need a small lift, since subpixel
     * snapping can move their plane) and never write depth; each tile draws them after all depth-writing
     * geometry, back to front by triangle depth. Opacity 0..255 rounds to a
     * source weight of 0..32: ALPHA mixes over the destination, ADD adds the
     * weighted source with per-channel saturation, MULTIPLY mixes the
     * per-channel product over the destination. */
    PROTO_3D_MATERIAL_BLEND_ALPHA=8u,
    PROTO_3D_MATERIAL_BLEND_ADD=16u,
    PROTO_3D_MATERIAL_BLEND_MULTIPLY=32u,
    /* Exempt from frame fog (skies, markers). Requires CAP_FOG. */
    PROTO_3D_MATERIAL_NO_FOG=64u,
    PROTO_3D_MATERIAL_VERTEX_COLOR=128u, /* requires mesh colors; multiplies vertex light */
    /* Nearest: floor(clamp(UV,0,1)*side), capped at side-1. Each axis is
     * independent; an unset axis repeats. Clamp is to the whole texture. */
    PROTO_3D_MATERIAL_CLAMP_U=256u,
    PROTO_3D_MATERIAL_CLAMP_V=512u
};
/* Proto3DMeshHeader_t.flags: at most one normal source, plus optional COLORS. LIT materials need
 * one. FLAT gives each triangle its face normal (counter-clockwise front);
 * SMOOTH averages area-weighted face normals per position; EXPLICIT reads
 * position_count Proto3DNormal_t at normals_offset, after the triangles. */
enum {
    PROTO_3D_MESH_NORMALS_FLAT=1u, PROTO_3D_MESH_NORMALS_SMOOTH=2u,
    PROTO_3D_MESH_NORMALS_EXPLICIT=4u,
    PROTO_3D_MESH_COLORS=8u /* position_count colors follow optional explicit normals */
};
/* Proto3DFrameHeader_t.flags: optional blocks follow the header in this
 * order, then the instances: Proto3DLighting_t, then Proto3DFog_t. */
enum { PROTO_3D_FRAME_LIGHTING=1u, PROTO_3D_FRAME_FOG=2u, PROTO_3D_FRAME_HUD=4u,
       PROTO_3D_FRAME_COVERAGE=8u,
       /* Performance mode (CAP_SCALE), at most one: the scene renders at
        * 240x180 or 160x120 and is expanded nearest-neighbour to 320x240;
        * the HUD is drawn afterwards at full resolution. Per frame, so an
        * app may switch at any time. */
       PROTO_3D_FRAME_SCALE_3_4=16u, PROTO_3D_FRAME_SCALE_1_2=32u };
#define PROTO_3D_MAX_HUD_COMMANDS 256u
#define PROTO_3D_MAX_HUD_TEXT 4096u
/* Appended AFTER instances: header, Game2D V2 commands, ASCII text
 * bytes (same glyph semantics as Game2D). No padding or trailing bytes.
 * Supports FILL_RECT, TEXT, CLIP_PUSH/POP, SMOOTH_SHAPE; CLEAR, assets and
 * cached layers are rejected. HUD pixels blend over the completed 3D frame. */
typedef struct { uint32_t version,bytes,command_count,text_bytes; } Proto3DHudHeader_t;
/* Proto3DFog_t.flags. DEPTH (requires CAP_ORTHOGRAPHIC) fogs by the sample's
 * normalized depth z/w instead of clip w: 0 at the near plane, 1 at the far
 * plane, linear in view depth for orthographic cameras (whose w is constant,
 * so default fog would be uniform). start/end are then in that 0..1 range. */
enum { PROTO_3D_FOG_DEPTH=1u };
enum { PROTO_3D_LIGHT_DIRECTIONAL=1u, PROTO_3D_LIGHT_POINT=2u };

typedef struct { float x,y,z; } Proto3DPosition_t;
typedef struct { float x,y,z; } Proto3DNormal_t; /* any nonzero length; normalized */
typedef struct { float u,v; } Proto3DUV_t;
/* Linear RGB modulation, 0..255 per channel; white is the identity. */
typedef struct { uint8_t r,g,b,reserved; } Proto3DColor_t;
typedef struct {
    uint16_t position[3],uv[3];
    uint16_t material,reserved;
} Proto3DTriangle_t;

/* Control records live in registered buffers. The adapter must snapshot this
 * record and pin/validate output before any state-changing operation. Handle
 * and frame_id are used only by DESTROY / QUERY_FRAME respectively. flags is
 * zero, or PROTO_3D_REQUEST_WAIT on SUBMIT (CAP_SUBMIT_WAIT) with wait_ms
 * 1..PROTO_3D_MAX_SUBMIT_WAIT_MS: while a frame is queued, the calling thread
 * sleeps until the engine takes it, then the submit is admitted; BUSY only
 * once wait_ms passed. Callers served on a shared kernel task (M-mode) never
 * wait and see BUSY at once. wait_ms is zero otherwise. */
enum { PROTO_3D_REQUEST_WAIT=1u };
typedef struct {
    uint32_t version,bytes,op,surface_id;
    uint32_t input_buffer,input_offset,input_bytes;
    uint32_t output_buffer,output_offset,output_bytes;
    uint32_t handle,frame_id,flags;
    union { uint32_t reserved,wait_ms; };
} Proto3DRequest_t;
typedef struct {
    uint32_t version,bytes;
    int32_t status;
    uint32_t handle,frame_id,frame_state;
    uint32_t accepted_id,completed_id,presented_id;
    uint32_t resource_bytes,transient_bytes,resources;
    uint32_t reserved[4];
} Proto3DResult_t;
typedef struct {
    uint32_t version,bytes,capabilities,width,height,tile_width,tile_height;
    uint32_t max_instances,max_positions,max_uvs,max_input_triangles;
    uint32_t max_clipped_triangles,max_tile_refs,max_bindings,max_resources;
    uint32_t max_texture_side,resource_bytes,transient_bytes,max_candidate_pixels;
    uint32_t queue_depth; /* one executing + at most one pending */
} Proto3DCaps_t;
typedef struct {
    uint32_t version,bytes,width,height,flags,resource_bytes,transient_bytes,reserved;
    char title[48];
} Proto3DOpen_t;

/* Canonical upload: header, positions, UVs, triangles, optional explicit
 * normals, optional position colors, no gaps or trailing bytes.
 * Counts bound every size calculation. Material indices select binding slots;
 * no material handles are baked into meshes. Meshes are immutable. */
typedef struct {
    uint32_t version,bytes,position_count,uv_count,triangle_count,material_slots;
    uint32_t positions_offset,uvs_offset,triangles_offset,flags;
    uint32_t normals_offset; /* EXPLICIT only, else zero */
    union { uint32_t reserved,colors_offset; }; /* COLORS only; otherwise zero */
} Proto3DMeshHeader_t;
/* Header then tightly packed native RGB565 (little-endian) texels; INDEX8
 * instead carries 256 RGB565 entries followed by width*height index bytes. */
typedef struct {
    uint32_t version,bytes,width,height,format,flags,pixels_offset,reserved;
} Proto3DTextureHeader_t;
/* CAP_MIPMAPS: power-of-two base dimensions; append all smaller levels down
 * to 1x1, halving each axis with a minimum of one. Base encoding is format;
 * every appended level is tightly packed little-endian RGB565. No offsets,
 * gaps, per-level handles or palettes. Legacy flags=0 packets are unchanged. */
#define PROTO_3D_TEXTURE_MIPMAPS 1u
#define PROTO_3D_MAX_TEXTURE_BYTES 174794u /* header + full 256x256 RGB565 chain */
static inline uint32_t proto_3d_texture_packet_bytes(uint32_t w,uint32_t h,uint32_t format,uint32_t flags) {
    if (!w || !h || w>PROTO_3D_MAX_TEXTURE_SIDE || h>PROTO_3D_MAX_TEXTURE_SIDE
        || (format!=PROTO_3D_TEXTURE_RGB565 && format!=PROTO_3D_TEXTURE_INDEX8)
        || (flags&~PROTO_3D_TEXTURE_MIPMAPS)) return 0;
    uint32_t n=(uint32_t)sizeof(Proto3DTextureHeader_t)
        +(format==PROTO_3D_TEXTURE_INDEX8?512u+w*h:2u*w*h);
    if (flags&PROTO_3D_TEXTURE_MIPMAPS) {
        if ((w&(w-1u)) || (h&(h-1u))) return 0;
        /* w,h<=256; <=8 iterations; total <=174794, all math fits uint32. */
        while (w>1u || h>1u) {
            w=w>1u?w>>1:1u;h=h>1u?h>>1:1u;n+=2u*w*h;
        }
    }
    return n;
}
typedef struct {
    uint32_t version,bytes,texture,flags;
    uint16_t color;
    uint8_t light;
    union { uint8_t opacity,reserved0; }; /* 0..255 with DITHERED_ALPHA or a BLEND mode; else zero */
    uint32_t reserved1;
} Proto3DMaterial_t;

/* Header, optional lighting/fog, draw_count instances, optional coverage, optional HUD.
 * FRAME_COVERAGE adds draw_count uint32 masks immediately after the instances.
 * Only bits 0..15 are valid; bit ((y&3)*4+(x&3)) permits a screen sample.
 * Zero discards every sample; 0xffff is full coverage. Coverage intersects
 * material dithering and preserves blend weights/depth rules. Requires CAP_COVERAGE.
 * Without the flag all draws have full coverage. Matrices are column-major column
 * vector transforms. Camera = perspective * view; model is affine. Frame IDs
 * strictly increase within one surface generation; rejected submissions do
 * not consume an ID. Empty draw lists clear the frame. */
typedef struct {
    uint32_t version,bytes,frame_id,draw_count,flags,background;
    float camera[16];
    uint32_t reserved[2];
} Proto3DFrameHeader_t;
/* Linear depth fog for every material without NO_FOG. A sample's weight is
 * clamp((w-start)/(end-start),0,1) of its perspective-correct clip w (view
 * depth for proto_3d_frustum cameras), rounded to 1/32; with
 * PROTO_3D_FOG_DEPTH, of its normalized depth z/w instead. Fogged color moves
 * toward color (ADD toward black, MULTIPLY toward white) before blending.
 * 0 <= start, start + 1e-6 <= end <= 1e6; color is RGB565 in the low bits.
 * flags is zero or PROTO_3D_FOG_DEPTH. */
typedef struct {
    uint32_t version,bytes,flags,color;
    float start,end;
    uint32_t reserved[2];
} Proto3DFog_t;
/* World-space lights, evaluated per vertex for LIT materials. Colors are
 * linear 0..16 per channel. Directional vector: the direction light travels
 * (any nonzero length). Point vector: position; range > 0 bounds a smooth
 * (1 - d^2/range^2)^2 falloff. Unused lights and reserved fields are zero. */
typedef struct {
    uint32_t type;
    float color[3],vector[3],range;
} Proto3DLight_t;
typedef struct {
    uint32_t version,bytes,count,flags;
    float ambient[3];
    uint32_t reserved;
    Proto3DLight_t lights[PROTO_3D_MAX_LIGHTS];
} Proto3DLighting_t;
typedef struct {
    uint32_t mesh,material_count,materials[PROTO_3D_MAX_BINDINGS];
    float model[16];
} Proto3DInstance_t;

#ifdef __cplusplus
#define PROTO_3D_ASSERT static_assert
#else
#define PROTO_3D_ASSERT _Static_assert
#endif
PROTO_3D_ASSERT(sizeof(float)==4 && FLT_RADIX==2 && FLT_MANT_DIG==24,"binary32 wire floats");
PROTO_3D_ASSERT(sizeof(Proto3DHudHeader_t)==16,"HUD header layout");
PROTO_3D_ASSERT(sizeof(ProtoGameDrawCommandV2_t)==32,"shared Game2D HUD command layout");
PROTO_3D_ASSERT(sizeof(Proto3DPosition_t)==12,"position layout");
PROTO_3D_ASSERT(sizeof(Proto3DUV_t)==8,"UV layout");
PROTO_3D_ASSERT(sizeof(Proto3DTriangle_t)==16,"triangle layout");
PROTO_3D_ASSERT(sizeof(Proto3DRequest_t)==56,"request layout");
PROTO_3D_ASSERT(offsetof(Proto3DRequest_t,wait_ms)==52,"wait_ms reuses reserved");
PROTO_3D_ASSERT(sizeof(Proto3DResult_t)==64,"result layout");
PROTO_3D_ASSERT(sizeof(Proto3DCaps_t)==80,"caps layout");
PROTO_3D_ASSERT(sizeof(Proto3DOpen_t)==80,"open layout");
PROTO_3D_ASSERT(sizeof(Proto3DMeshHeader_t)==48,"mesh header layout");
PROTO_3D_ASSERT(sizeof(Proto3DTextureHeader_t)==32,"texture header layout");
PROTO_3D_ASSERT(sizeof(Proto3DMaterial_t)==24,"material layout");
PROTO_3D_ASSERT(sizeof(Proto3DFrameHeader_t)==96,"frame header layout");
PROTO_3D_ASSERT(sizeof(Proto3DInstance_t)==328,"instance layout");
PROTO_3D_ASSERT(sizeof(Proto3DNormal_t)==12,"normal layout");
PROTO_3D_ASSERT(sizeof(Proto3DColor_t)==4,"vertex color layout");
PROTO_3D_ASSERT(offsetof(Proto3DMeshHeader_t,colors_offset)==44,"colors reuse reserved offset");
PROTO_3D_ASSERT(sizeof(Proto3DLight_t)==32,"light layout");
PROTO_3D_ASSERT(sizeof(Proto3DLighting_t)==288,"lighting layout");
PROTO_3D_ASSERT(sizeof(Proto3DFog_t)==32,"fog layout");
PROTO_3D_ASSERT(offsetof(Proto3DMeshHeader_t,normals_offset)==40,"normals offset");
PROTO_3D_ASSERT(offsetof(Proto3DFrameHeader_t,camera)==24,"camera offset");
PROTO_3D_ASSERT(offsetof(Proto3DInstance_t,model)==264,"model offset");
PROTO_3D_ASSERT(offsetof(Proto3DMaterial_t,color)==16,"color offset");
PROTO_3D_ASSERT(offsetof(Proto3DMaterial_t,opacity)==19,"opacity reuses reserved byte");
#undef PROTO_3D_ASSERT
