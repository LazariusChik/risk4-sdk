#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "proto_game3d.h"
#include "proto_abi.h"
#ifdef __cplusplus
extern "C" {
#endif

/* SDK state is app-owned; serialize access to a client across app threads.
 * Initialize a fresh client to zero. Upload arrays are copied synchronously;
 * mesh/material handles remain immutable until destroyed. */
typedef struct {
    void *storage;
    ProtoBufferHandle_t buffer;
    uint32_t surface,capacity,next_frame,draws;
    bool building,closing,lighting,fog,coverage;
    uint8_t scale; /* PROTO_3D_SCALE_*, kept across frames */
    int error;
    /* HUD frames: one registered send packet, kept and grown on demand. */
    void *packet;
    ProtoBufferHandle_t packet_buffer;
    uint32_t packet_bytes;
    uint32_t capabilities; /* PROTO_3D_CAP_* of the running kernel, from init */
} Proto3DClient;
/* Mesh upload. normals is 0 or one PROTO_3D_MESH_NORMALS_* source; EXPLICIT
 * reads position_count entries from explicit_normals. LIT materials need a
 * mesh with normals (and PROTO_3D_CAP_LIGHTING). */
typedef struct {
    const Proto3DPosition_t *positions; uint32_t position_count;
    const Proto3DUV_t *uvs; uint32_t uv_count;
    const Proto3DTriangle_t *triangles; uint32_t triangle_count;
    uint32_t material_slots, normals;
    const Proto3DNormal_t *explicit_normals;
} Proto3DMeshDesc;
int proto_3d_query_caps(Proto3DCaps_t *caps);
int proto_3d_init(Proto3DClient *client,const char *title,uint32_t instance_capacity);
/* Cancel and wait up to timeout_ms for retirement, then release SDK storage.
 * TIMEOUT preserves the client so deinit/query can be retried. */
int proto_3d_deinit(Proto3DClient *client,uint32_t timeout_ms);
int proto_3d_upload_mesh(Proto3DClient *client,const Proto3DPosition_t *positions,uint32_t position_count,
                         const Proto3DUV_t *uvs,uint32_t uv_count,const Proto3DTriangle_t *triangles,
                         uint32_t triangle_count,uint32_t material_slots,uint32_t *handle);
int proto_3d_create_mesh(Proto3DClient *client,const Proto3DMeshDesc *mesh,uint32_t *handle);
/* CAP_VERTEX_COLOR: position_count RGB8 colors, each reserved byte zero.
 * Optional explicit normals precede colors in the canonical wire upload.
 * Mesh descriptors and existing entrypoints retain their source/binary layout. */
int proto_3d_create_mesh_colored(Proto3DClient *client,const Proto3DMeshDesc *mesh,
                                const Proto3DColor_t *colors,uint32_t *handle);
int proto_3d_upload_texture(Proto3DClient *client,const uint16_t *pixels,uint32_t width,uint32_t height,uint32_t *handle);
/* CAP_MIPMAPS: canonical complete packet, including its texture header.
 * Inputs are copied synchronously by the service; no runtime mip generation. */
int proto_3d_upload_texture_mipmapped(Proto3DClient *client,const void *packet,uint32_t bytes,uint32_t *handle);
/* CAP_INDEX8: copies exactly 256 RGB565 colors and width*height indices. */
int proto_3d_upload_texture_index8(Proto3DClient *client,const uint8_t *indices,
    const uint16_t palette[256],uint32_t width,uint32_t height,uint32_t *handle);
int proto_3d_create_material(Proto3DClient *client,uint32_t texture,uint16_t color,uint8_t light,uint32_t *handle);
/* flags: PROTO_3D_MATERIAL_*. With LIT, light is a 0..255 strength. */
int proto_3d_create_material_ex(Proto3DClient *client,uint32_t texture,uint16_t color,uint8_t light,
                                uint32_t flags,uint32_t *handle);
/* Requires CAP_DITHERED_ALPHA. Opacity 0..255 rounds to 17 coverage levels.
 * Kept samples write LESS depth; holes leave depth/color untouched. flags may
 * additionally select LIT and CULL_BACK. Immutable: create handles once and
 * switch bindings, rather than creating a new resource every frame. */
int proto_3d_create_material_dithered(Proto3DClient *client,uint32_t texture,uint16_t color,uint8_t light,
                                      uint32_t flags,uint8_t opacity,uint32_t *handle);
/* Requires CAP_BLEND. flags names exactly one PROTO_3D_MATERIAL_BLEND_* mode
 * and may add LIT, CULL_BACK and NO_FOG. Opacity 0..255 rounds to 33 source
 * weights; zero draws nothing. Blended surfaces test depth (equal depth passes) but
 * never write it and draw back to front after the depth-writing geometry. */
int proto_3d_create_material_blended(Proto3DClient *client,uint32_t texture,uint16_t color,uint8_t light,
                                     uint32_t flags,uint8_t opacity,uint32_t *handle);
int proto_3d_destroy(Proto3DClient *client,uint32_t handle);
int proto_3d_frame_begin(Proto3DClient *client,const float camera[16],uint16_t background);
/* Performance mode: the scene renders at a lower resolution and is scaled
 * up to 320x240 (nearest neighbour); the HUD stays full resolution. Applies
 * from the next submitted frame until changed, so it can be switched at any
 * time. FULL always succeeds; the others need CAP_SCALE (NOT_SUPPORTED). */
enum { PROTO_3D_SCALE_FULL=0u, PROTO_3D_SCALE_3_4=1u, PROTO_3D_SCALE_1_2=2u };
int proto_3d_set_scale(Proto3DClient *client,uint32_t scale);
int proto_3d_draw(Proto3DClient *client,uint32_t mesh,const uint32_t *materials,uint32_t material_count,const float model[16]);
/* Literal screen-aligned 4x4 sample mask; see FRAME_COVERAGE. Zero is a
 * no-op; partial coverage needs CAP_COVERAGE. Materials remain immutable. */
int proto_3d_draw_coverage(Proto3DClient *client,uint32_t mesh,const uint32_t *materials,
                         uint32_t material_count,const float model[16],uint16_t coverage);
/* Frame lights (after frame_begin, world space, linear 0..16 per channel).
 * Any call adds the lighting block; without one, LIT materials see ambient
 * 1 and no lights. Directional: the direction light travels. Point: a
 * position and the range where its smooth falloff reaches zero. More than
 * PROTO_3D_MAX_LIGHTS lights is NO_SPACE (the frame then fails to submit). */
int proto_3d_frame_ambient(Proto3DClient *client,const float color[3]);
int proto_3d_frame_directional(Proto3DClient *client,const float color[3],const float direction[3]);
int proto_3d_frame_point(Proto3DClient *client,const float color[3],const float position[3],float range);
/* Requires CAP_FOG; after frame_begin, applies to this frame. Linear fog by
 * view depth (clip w): none before start, full RGB565 color from end on.
 * 0 <= start, start + 1e-6 <= end <= 1e6, else the submit is INVALID.
 * Materials created with PROTO_3D_MATERIAL_NO_FOG are exempt. */
int proto_3d_frame_fog(Proto3DClient *client,uint16_t color,float start,float end);
/* proto_3d_frame_fog() with Proto3DFog_t flags: PROTO_3D_FOG_DEPTH (requires
 * CAP_ORTHOGRAPHIC) fogs by normalized depth, start/end in 0..1; for a
 * proto_3d_orthographic camera a view distance d is (d-near)/(far-near). */
int proto_3d_frame_fog_ex(Proto3DClient *client,uint16_t color,float start,float end,uint32_t flags);
/* BUSY keeps the built frame and its ID available for retry. */
/* CAP_HUD: snapshots Game2D V2 text, fills, smooth shapes and clip commands.
 * Call after frame_begin; replaces this frame's HUD. No CLEAR/assets/layers.
 * Invalid/overflowing HUDs reject the complete frame; BUSY retries preserve it. */
int proto_3d_frame_hud(Proto3DClient *client,const ProtoGameDrawCommandV2_t *commands,
    uint32_t count,const char *text,uint32_t text_bytes);
int proto_3d_frame_submit(Proto3DClient *client,uint32_t *frame_id);
/* proto_3d_frame_submit() that waits up to wait_ms (1..PROTO_3D_MAX_SUBMIT_WAIT_MS)
 * for queue space instead of returning BUSY. With CAP_SUBMIT_WAIT on a U-mode
 * thread the thread sleeps in the kernel until the engine takes the queued
 * frame, so a render-bound loop is paced by the engine with no polling. Older
 * kernels and M-mode callers retry every millisecond instead. BUSY after
 * wait_ms keeps the built frame for another call, as with frame_submit. */
int proto_3d_frame_submit_wait(Proto3DClient *client,uint32_t *frame_id,uint32_t wait_ms);
int proto_3d_query_frame(Proto3DClient *client,uint32_t frame_id,Proto3DResult_t *result);

/* Column-major matrices, column vectors, right-handed, camera down -Z,
 * 0<=clip-z<=w. Camera = projection * view. Checked helpers preserve output
 * on failure and allow input/output aliasing. No libc/libm dependency. */
void proto_3d_identity(float matrix[16]);
bool proto_3d_multiply(float out[16],const float a[16],const float b[16]);
bool proto_3d_frustum(float out[16],float left,float right,float bottom,float top,float near_plane,float far_plane);
/* View box [left,right] x [bottom,top] at view distances near..far (near may
 * be zero or negative). Clip w is 1, so parallel lines stay parallel; clip z
 * is (distance-near)/(far-near). Values finite and within +/-1e6, left < right,
 * bottom < top, near < far. Use PROTO_3D_FOG_DEPTH for fog with this camera:
 * default fog reads w and would be uniform. Rendering needs CAP_ORTHOGRAPHIC. */
bool proto_3d_orthographic(float out[16],float left,float right,float bottom,float top,float near_plane,float far_plane);
bool proto_3d_look_at(float out[16],const float eye[3],const float target[3],const float up[3]);

/* Shape builders (proto_3d_shape.c, loaded apps). They write into caller
 * arrays and never allocate. With every array NULL a builder only reports
 * the three counts. Arrays smaller than the counts are NO_SPACE (nothing
 * written, counts set to what is needed); parameters whose counts exceed a
 * kernel limit, or non-finite/non-positive sizes, are INVALID. normals may
 * be NULL when the arrays are given. Shapes are centered on the origin, +Y
 * up, counter-clockwise front faces seen from outside, unit normals. Hard
 * edges duplicate positions; seams duplicate only UVs. One material slot. */
typedef struct {
    Proto3DPosition_t *positions; Proto3DNormal_t *normals;
    Proto3DUV_t *uvs; Proto3DTriangle_t *triangles;
    uint32_t position_capacity,uv_capacity,triangle_capacity;
    uint32_t position_count,uv_count,triangle_count; /* outputs */
} Proto3DShape;
/* XZ plane facing +Y, UV (0,0) at (-x,-z). Segments 1..64 per side. */
int proto_3d_shape_plane(Proto3DShape *shape,float width,float depth,uint32_t segments_x,uint32_t segments_z);
/* 1x1 quad in the XY plane facing +Z, UV (0,0) top-left: the billboard mesh. */
int proto_3d_shape_quad(Proto3DShape *shape);
/* 24 positions, each face's UVs spanning 0..1, upright seen from outside. */
int proto_3d_shape_box(Proto3DShape *shape,float width,float height,float depth);
/* segments 3..64 around +Y, rings 2..32 pole to pole; u wraps from +X
 * toward -Z, v = 0 at the top. */
int proto_3d_shape_sphere(Proto3DShape *shape,float radius,uint32_t segments,uint32_t rings);
/* segments 3..128; caps add flat discs with planar UVs. */
int proto_3d_shape_cylinder(Proto3DShape *shape,float radius,float height,uint32_t segments,bool caps);
/* Uploads a built shape: explicit normals when present, else SMOOTH. */
int proto_3d_create_shape_mesh(Proto3DClient *client,const Proto3DShape *shape,uint32_t *handle);
enum { PROTO_3D_BILLBOARD_SPHERICAL=1u, PROTO_3D_BILLBOARD_CYLINDRICAL=2u };
/* Model matrix for the quad: scaled to width x height at position and turned
 * toward the camera of view (world-to-view, as from proto_3d_look_at).
 * SPHERICAL faces the camera fully; CYLINDRICAL turns only about world +Y
 * (trees, signs). Preserves out on failure. */
bool proto_3d_billboard(float out[16],const float view[16],const float position[3],
                        float width,float height,uint32_t mode);

/* Quaternions and transforms (proto_3d_anim.c, loaded apps; docs/game3d/
 * reference/game3d-animation.md). Quaternions are (x, y, z, w) as in glTF,
 * unit length, rotating counter-clockwise about their axis seen from its
 * tip. No libm; outputs may alias inputs. */
void proto_3d_quat_identity(float q[4]);
/* Rotation of radians about axis (any nonzero finite length). */
bool proto_3d_quat_axis_angle(float q[4],const float axis[3],float radians);
/* Scales to unit length; false (q kept) for zero or non-finite. */
bool proto_3d_quat_normalize(float q[4]);
/* a * b: rotate by b, then by a. */
void proto_3d_quat_multiply(float out[4],const float a[4],const float b[4]);
/* Shortest-path spherical interpolation of unit quaternions, t clamped to
 * 0..1. Within 5e-7 rad of exact (measured 3.5e-7 over 200,000 pairs);
 * keys closer than 0.0141 rad of arc use normalized lerp, equally exact. */
void proto_3d_quat_slerp(float out[4],const float a[4],const float b[4],float t);
/* out = T * R * S, the glTF node transform. q must be unit. */
bool proto_3d_compose(float out[16],const float translation[3],const float rotation[4],const float scale[3]);

/* Ray casts and bounds (proto_3d_ray.c, loaded apps; docs/game3d/reference/
 * game3d-raycast.md). Pure SDK code in user space: no kernel call, no
 * capability, no allocation per query. Picking, line of sight, aiming and
 * "what is below me" all use proto_3d_raycast. */
typedef struct {
    float min[3],max[3];      /* axis-aligned box */
    float center[3],radius;   /* sphere around the box center holding every point */
} Proto3DBounds;
/* Exact box of n >= 1 finite positions; the sphere is centered on the box. */
bool proto_3d_bounds_compute(Proto3DBounds *out,const Proto3DPosition_t *positions,uint32_t count);
/* Bounds of in after an affine model matrix (bottom row 0 0 0 1): a box
 * holding the transformed box, and the sphere scaled by the largest axis
 * scale. Preserves out on failure; in and out may alias. */
bool proto_3d_bounds_transform(Proto3DBounds *out,const Proto3DBounds *in,const float model[16]);

/* Ray mesh: an SDK-side copy of a mesh's positions and triangles with its
 * bounds and a bounding-volume hierarchy, built once by create. Zero-
 * initialize; destroy in any state. It is independent of the kernel mesh,
 * so the same geometry may be uploaded, ray cast, or both. Up to 65535
 * positions and triangles; every triangle index must be < position_count.
 * Heap: 12 bytes per position, 8 per triangle, about 8 per triangle of tree. */
typedef struct Proto3DRayMeshData Proto3DRayMeshData;
typedef struct {
    Proto3DRayMeshData *data;
    Proto3DBounds bounds;     /* mesh space */
    uint32_t bytes;           /* heap held by data */
} Proto3DRayMesh;
int proto_3d_ray_mesh_create(Proto3DRayMesh *mesh,const Proto3DPosition_t *positions,uint32_t position_count,
                             const Proto3DTriangle_t *triangles,uint32_t triangle_count);
void proto_3d_ray_mesh_destroy(Proto3DRayMesh *mesh);

/* Ray scene: app-owned instance storage. An instance places a ray mesh with
 * an affine model matrix (as passed to proto_3d_draw). Its world bounds and
 * inverse are computed when it is added or moved, never per ray. The mesh
 * must outlive every instance that uses it. mask selects instances per
 * query (a ray sees an instance when instance.mask & query mask != 0). */
typedef struct {
    const Proto3DRayMesh *mesh;
    uint32_t id,mask,mirrored;  /* mirrored: the model reverses winding */
    uint32_t identity;          /* the model is exactly the identity: no transform per triangle */
    float inverse[12];          /* world to mesh space, 3x4 column-major */
    float model[12];            /* mesh to world, 3x4 column-major (collision) */
    Proto3DBounds world;        /* world bounds */
} Proto3DRayInstance;
typedef struct {
    Proto3DRayInstance *instances;
    uint32_t capacity,count;
} Proto3DRayScene;
void proto_3d_ray_scene_init(Proto3DRayScene *scene,Proto3DRayInstance *storage,uint32_t capacity);
void proto_3d_ray_scene_clear(Proto3DRayScene *scene);
/* INVALID for a NULL/empty mesh, a non-affine or singular matrix, or
 * coordinates beyond +/-1e6; NO_SPACE when full. index may be NULL. */
int proto_3d_ray_scene_add(Proto3DRayScene *scene,const Proto3DRayMesh *mesh,const float model[16],
                           uint32_t id,uint32_t mask,uint32_t *index);
/* Moves an instance; on failure it keeps its previous placement. */
int proto_3d_ray_scene_move(Proto3DRayScene *scene,uint32_t index,const float model[16]);
int proto_3d_ray_scene_set_mask(Proto3DRayScene *scene,uint32_t index,uint32_t mask);

enum {
    PROTO_3D_RAY_CULL_BACK=1u,   /* ignore triangles seen from behind */
    PROTO_3D_RAY_ANY=2u,         /* stop at the first hit found, not the nearest (line of sight) */
};
enum { PROTO_3D_RAY_HIT_BACK=1u };  /* the ray struck the triangle's back face */
typedef struct {
    uint32_t instance;          /* index in the scene */
    uint32_t id;                /* that instance's id */
    uint32_t triangle;          /* triangle index in the mesh as created */
    uint32_t flags;             /* PROTO_3D_RAY_HIT_* */
    float distance;             /* world units along the normalized direction */
    float point[3];             /* world hit point */
    float normal[3];            /* unit world normal of the triangle's front face */
} Proto3DRayHit;
/* Nearest triangle hit with 0 < distance <= max_dist along dir (any nonzero
 * length; normalized here). Every instance is a candidate. Returns true on a
 * hit and fills *hit (hit may be NULL); a miss or invalid input (NULL scene,
 * non-finite or |origin| > 1e6, zero dir, max_dist <= 0) returns false and
 * leaves *hit unchanged. Bounds reject first: instance sphere, then the
 * mesh's tree; triangles are tested exactly (Moller-Trumbore, no division
 * per triangle). Shared edges may report either triangle. */
bool proto_3d_raycast(const Proto3DRayScene *scene,const float origin[3],const float dir[3],
                      float max_dist,Proto3DRayHit *hit);
/* With an instance mask and PROTO_3D_RAY_* flags. */
bool proto_3d_raycast_ex(const Proto3DRayScene *scene,const float origin[3],const float dir[3],
                         float max_dist,uint32_t mask,uint32_t flags,Proto3DRayHit *hit);
/* Line of sight: true when any masked triangle lies strictly between from
 * and to (two-sided). Equal points are never blocked. */
bool proto_3d_ray_blocked(const Proto3DRayScene *scene,const float from[3],const float to[3],uint32_t mask);
/* Collision (proto_3d_collide.c; docs/game3d/reference/game3d-collision.md).
 * Spheres and capsules against the triangles of a ray scene, using the same
 * bounds and trees as ray casts. Triangles are two-sided. A contact's normal
 * is unit, from the triangle toward the shape; depth = radius - distance (> 0
 * when overlapping). */
typedef struct {
    uint32_t instance,id,triangle;
    float depth;
    float normal[3];
    float point[3];            /* closest point on the triangle */
} Proto3DContact;
/* The deepest contact within radius of the segment a-b (a capsule) or of a
 * point (a sphere); false when nothing is that close or the input is invalid
 * (non-finite, beyond +/-1e6, radius outside 0..1e4). */
bool proto_3d_capsule_contact(const Proto3DRayScene *world,const float a[3],const float b[3],float radius,
                              uint32_t mask,Proto3DContact *deepest);
bool proto_3d_sphere_contact(const Proto3DRayScene *world,const float center[3],float radius,uint32_t mask,
                             Proto3DContact *deepest);

/* Character controller: an upright capsule that walks, slides along walls,
 * climbs walkable slopes and steps, snaps down stairs, and reports ground.
 * Gravity, jumping and input are the caller's: pass the velocity you want,
 * get back the velocity that survived the collisions. Zero-initialize, set
 * the shape, and keep the struct between frames (grounded is state). */
typedef struct {
    float position[3];         /* the feet: lowest point of the capsule */
    float radius,height;       /* 0 < radius <= 1e3; 2 * radius <= height <= 1e4 */
    float step_height;         /* ledges up to this climb while grounded; 0 disables (<= height) */
    float max_slope;           /* cosine of the steepest walkable slope, 0.05..1 (0.7: 45.6 degrees) */
    float snap;                /* a grounded character follows ground up to this far below; 0 disables */
    uint32_t mask;             /* scene instances that block */
    bool grounded;             /* written: standing on walkable ground after the move */
    float ground_normal[3];    /* written: zero when not grounded */
} Proto3DCharacter;
enum {
    PROTO_3D_MOVE_GROUNDED=1u,
    PROTO_3D_MOVE_WALL=2u,     /* slid along a wall or a too-steep slope */
    PROTO_3D_MOVE_CEILING=4u,  /* struck something overhead */
    PROTO_3D_MOVE_STEPPED=8u,  /* climbed a step */
    PROTO_3D_MOVE_SNAPPED=16u, /* followed the ground down */
    PROTO_3D_MOVE_LIMITED=32u, /* the move was longer than 16 radii and was cut short */
    PROTO_3D_MOVE_STUCK=64u,   /* still overlapping after the solver's iterations */
};
typedef struct {
    uint32_t flags;            /* PROTO_3D_MOVE_* */
    uint32_t ground_id;        /* id of the instance underfoot when grounded */
    float ground_normal[3],wall_normal[3];
    float moved[3];            /* the displacement actually made */
} Proto3DMoveResult;
/* Moves character by velocity * dt (0 <= dt <= 1) against the masked
 * instances of world; updates position, grounded, ground_normal and
 * velocity (components into surfaces removed; a landing zeroes the fall).
 * INVALID (nothing changed) for bad shapes or non-finite input. A move of up
 * to 16 radii per call is resolved in half-radius substeps, so nothing
 * thicker than the radius is tunnelled; more is cut short (LIMITED). */
int proto_3d_move_and_slide(const Proto3DRayScene *world,Proto3DCharacter *character,float velocity[3],float dt,
                            Proto3DMoveResult *result);

/* Bounds query: indices of masked instances whose world box overlaps
 * [min,max]. Writes up to capacity indices in scene order and returns how
 * many overlap in all (more than capacity means some were left out). */
uint32_t proto_3d_ray_scene_overlap(const Proto3DRayScene *scene,const float min[3],const float max[3],
                                    uint32_t mask,uint32_t *indices,uint32_t capacity);

/* .g3a assets (proto_3d_asset.c, loaded apps; docs/game3d/reference/game3d-assets.md). An
 * asset is a resource made by tools/g3a_convert.py. It is either fully
 * resident or owns nothing: any failure destroys what it created. Zero-
 * initialize a Proto3DAsset; release it in any state. */
typedef struct Proto3DAssetState Proto3DAssetState;
typedef struct { Proto3DAssetState *state; } Proto3DAsset;
enum { PROTO_3D_ASSET_TEXTURE=1u, PROTO_3D_ASSET_MESH=2u, PROTO_3D_ASSET_MATERIAL=3u, PROTO_3D_ASSET_SCENE=4u,
       PROTO_3D_ASSET_ANIMATION=5u };
typedef struct {
    uint32_t sections,resources;  /* sections in the file; kernel handles created */
    uint32_t resource_bytes;      /* the asset's share of the Game3D resource quota */
    uint32_t heap_peak;           /* most SDK heap held at once while loading */
    uint32_t load_us;             /* start to resident (or failure) */
    int32_t error;                /* 0, or why it failed */
    uint32_t error_section;       /* failing section index, or UINT32_MAX */
    char error_name[32];          /* that section's name, if known */
    /* Streaming progress for loading bars: bytes decoded so far and the
     * asset's size (0 until the resource is open). Safe to read while a
     * preload runs. */
    uint32_t bytes_read,bytes_total;
} Proto3DAssetInfo;
/* Blocking: read, upload and resolve the whole asset (a loading screen). */
int proto_3d_asset_load(Proto3DClient *client,Proto3DAsset *asset,const char *resource);
/* Starts reading on an SDK worker thread and returns at once (the build tool
 * grants thread-create and resource for this call). Without a worker, reads
 * happen inside poll, a few chunks per call. */
int proto_3d_asset_preload(Proto3DAsset *asset,const char *resource);
/* On the client's thread, e.g. once per frame: uploads at most one section
 * the reader has finished. PROTO_3D_BUSY while loading, KERNEL_OK once
 * resident, else the failure (the asset then owns nothing). */
int proto_3d_asset_poll(Proto3DClient *client,Proto3DAsset *asset);
/* Cancels any load, destroys every handle the asset created, frees its
 * memory and zeroes it. TIMEOUT (worker did not stop) keeps it for a retry. */
int proto_3d_asset_release(Proto3DClient *client,Proto3DAsset *asset);
/* Kernel handle of a resident TEXTURE, MESH or MATERIAL by name; else 0. */
uint32_t proto_3d_asset_find(const Proto3DAsset *asset,uint32_t type,const char *name);
/* Draws every mesh node of a scene (NULL: the first) with model applied on
 * top of the node transforms; one proto_3d_draw per mesh node. */
int proto_3d_asset_draw(Proto3DClient *client,const Proto3DAsset *asset,const char *scene,const float model[16]);
/* Screen-aligned 4x4 sample mask, shared by all nodes. Zero submits no draws;
 * 0xffff is ordinary drawing. Partial masks require CAP_COVERAGE. */
int proto_3d_asset_draw_coverage(Proto3DClient *client,const Proto3DAsset *asset,const char *scene,
                               const float model[16],uint16_t coverage);
int proto_3d_asset_info(const Proto3DAsset *asset,Proto3DAssetInfo *info);

/* Rigid node animation (milestone 4.5; docs/game3d/reference/game3d-animation.md).
 * An ANIMATION section is a clip over one scene: translation, rotation and
 * scale keys per animated node. Sampling a clip at a time interpolates them
 * (lerp, slerp, or STEP), rebuilds those nodes' local matrices, and walks the
 * scene's hierarchy into a pose: one world matrix per scene node. Nodes the
 * clip cannot move are copied from the load-time world matrices. A pose is
 * app storage, so many instances can play one clip at different times. No
 * allocation and no kernel call while sampling. */
typedef struct Proto3DClip Proto3DClip;
typedef struct {
    float (*world)[16];        /* capacity matrices, column-major */
    uint32_t capacity;
    uint32_t node_count;       /* written: the scene's node count */
    const void *scene;         /* written: which scene the pose belongs to */
} Proto3DPose;
typedef struct {
    float duration;            /* last key time over every channel, seconds */
    uint32_t tracks;           /* animated nodes */
    uint32_t nodes;            /* nodes in the clip's scene: the pose capacity needed */
    uint32_t moving;           /* nodes the clip can move (tracks and their descendants) */
    char scene[32];            /* the scene's name, for proto_3d_asset_draw */
} Proto3DClipInfo;
enum { PROTO_3D_CLIP_LOOP=1u };  /* wrap time into 0..duration; else clamp */
/* A resident clip by name, valid until the asset is released; else NULL. */
const Proto3DClip *proto_3d_asset_clip(const Proto3DAsset *asset,const char *name);
int proto_3d_clip_info(const Proto3DClip *clip,Proto3DClipInfo *info);
/* INVALID for a non-finite time or unknown flags; NO_SPACE when the pose
 * holds fewer than the scene's nodes. */
int proto_3d_clip_sample(const Proto3DClip *clip,float time,uint32_t flags,Proto3DPose *pose);
/* The load-time pose of a scene (NULL: the first). */
int proto_3d_asset_rest_pose(const Proto3DAsset *asset,const char *scene,Proto3DPose *pose);
/* Draws every mesh node of the pose's scene at model * pose world, one
 * proto_3d_draw per mesh node, as proto_3d_asset_draw does. */
int proto_3d_asset_draw_pose(Proto3DClient *client,const Proto3DAsset *asset,const Proto3DPose *pose,
                             const float model[16]);
int proto_3d_asset_draw_pose_coverage(Proto3DClient *client,const Proto3DAsset *asset,const Proto3DPose *pose,
                                      const float model[16],uint16_t coverage);
/* Opt-in load profiling (benchmarks). Each timed point reads proto_time_us(),
 * a kernel call on current firmware, so leave it off in shipping code.
 * RAYCAST also keeps a ray mesh for every MESH section (built once while
 * loading, freed by release; see Proto3DRayMesh for its heap cost). */
/* BASE_TEXTURES uploads only the base image of mipmapped textures, exactly
 * as on firmware without CAP_MIPMAPS (an app's "mipmaps off" setting). */
enum { PROTO_3D_ASSET_PROFILE=1u, PROTO_3D_ASSET_RAYCAST=2u, PROTO_3D_ASSET_BASE_TEXTURES=4u };
/* Mesh-space bounds of a resident MESH, computed once while loading (with
 * or without RAYCAST). NOT_FOUND for an unknown name. */
int proto_3d_asset_mesh_bounds(const Proto3DAsset *asset,const char *mesh,Proto3DBounds *out);
/* Bounds of a scene's mesh nodes (NULL: the first scene) under model, or of
 * one node when node < the scene's node count (UINT32_MAX: all of them).
 * NOT_FOUND when the scene or node has no mesh. */
int proto_3d_asset_bounds(const Proto3DAsset *asset,const char *scene,uint32_t node,
                          const float model[16],Proto3DBounds *out);
/* The asset's ray mesh for a MESH (loaded with RAYCAST), else NULL. */
const Proto3DRayMesh *proto_3d_asset_ray_mesh(const Proto3DAsset *asset,const char *mesh);
/* Adds one ray instance per mesh node of a scene (NULL: the first) with
 * model applied on top of the node transforms, as proto_3d_asset_draw
 * draws them; all get id and mask. NOT_SUPPORTED without RAYCAST; NO_SPACE
 * adds nothing. *first (optional) is the first new index. Release the asset
 * only after clearing ray scenes that use it. */
int proto_3d_asset_ray_add(Proto3DRayScene *rays,const Proto3DAsset *asset,const char *scene,
                           const float model[16],uint32_t id,uint32_t mask,uint32_t *first);
/* Moves the instances proto_3d_asset_ray_add made for the pose's scene,
 * starting at first, to model * pose world: shots follow an animated door. */
int proto_3d_asset_ray_pose(Proto3DRayScene *rays,uint32_t first,const Proto3DAsset *asset,
                            const Proto3DPose *pose,const float model[16]);
int proto_3d_asset_load_ex(Proto3DClient *client,Proto3DAsset *asset,const char *resource,uint32_t flags);
int proto_3d_asset_preload_ex(Proto3DAsset *asset,const char *resource,uint32_t flags);
typedef struct {
    uint32_t chunks,stored_bytes,bytes;      /* resource chunks read, as stored and decoded */
    uint32_t read_us,decode_us;              /* reader: proto_resource_read, chunk decode */
    uint32_t uploads,upload_us,upload_max_us,upload_max_section; /* committer, per kernel upload */
    uint32_t scene_us;                       /* building NODES sections */
    uint32_t polls,poll_us,poll_max_us;      /* proto_3d_asset_poll calls and their cost */
    uint32_t threaded;                       /* 1 when a worker did the reads */
    /* The slowest poll: which one (0-based), what it uploaded and read.
     * A slow poll that did little work was waiting, not working. */
    uint32_t poll_max_index,poll_max_uploads,poll_max_upload_bytes,poll_max_chunks;
    uint32_t core;                           /* PROTO_3D_ASSET_CORE_* actually used */
} Proto3DAssetProfile;
/* Complete once load/poll has returned something other than BUSY. Counts
 * are always kept; the *_us fields stay zero without PROTO_3D_ASSET_PROFILE. */
int proto_3d_asset_profile(const Proto3DAsset *asset,Proto3DAssetProfile *profile);

/* Where a preload reads. AUTO is the SDK's choice for this hardware
 * (docs/game3d/reference/game3d-assets.md#core-placement); CORE_0/CORE_1 pin the worker;
 * INLINE uses no thread: poll() reads too, within its budget. */
enum {
    PROTO_3D_ASSET_CORE_AUTO=0u, PROTO_3D_ASSET_CORE_0=1u, PROTO_3D_ASSET_CORE_1=2u,
    PROTO_3D_ASSET_CORE_ANY=3u,   /* let the scheduler place the worker */
    PROTO_3D_ASSET_CORE_INLINE=4u
};
/* Default per-poll budget: about how long one poll() may work. */
#define PROTO_3D_ASSET_DEFAULT_BUDGET_US 2000u
typedef struct {
    uint32_t flags;          /* PROTO_3D_ASSET_PROFILE */
    uint32_t core;           /* PROTO_3D_ASSET_CORE_* */
    /* Microseconds of work one poll() may do on the calling thread: it
     * uploads (and, INLINE, reads) until this much time has passed, always
     * at least one step. 0 = PROTO_3D_ASSET_DEFAULT_BUDGET_US;
     * UINT32_MAX = no limit (finish as soon as the reader allows). */
    uint32_t poll_budget_us;
    uint32_t reserved;       /* zero */
} Proto3DAssetOptions;
/* options NULL = all defaults. */
int proto_3d_asset_preload_opt(Proto3DAsset *asset,const char *resource,const Proto3DAssetOptions *options);
#ifdef __cplusplus
}
#endif
