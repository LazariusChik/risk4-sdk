#include "proto_loaded.h"
#include "proto_3d.h"

/* Shared process heap keeps upload data out of the entry thread's private
 * stack. The helpers copy uploads synchronously; client storage lives until
 * the service confirms retirement. No external model or texture is needed. */
typedef struct {
    Proto3DClient client;
    Proto3DPosition_t positions[24];
    Proto3DNormal_t normals[24];
    Proto3DUV_t uvs[24];
    Proto3DTriangle_t triangles[12];
} Scene;

static int wait_presented(Proto3DClient *client, uint32_t frame) {
    /* At most 100 queries and 1,000 ms of sleep; int32_t counter stays 0..100.
     * This is lifecycle polling, not a per-pixel or render loop. */
    for (int32_t attempt = 0; attempt < 100; ++attempt) {
        Proto3DResult_t result = {0};
        int rc = proto_3d_query_frame(client, frame, &result);
        if (rc != KERNEL_OK) return rc;
        if (result.frame_state == PROTO_3D_FRAME_PRESENTED) return KERNEL_OK;
        if (result.frame_state == PROTO_3D_FRAME_CANCELLED ||
            result.frame_state == PROTO_3D_FRAME_REJECTED) return -1;
        proto_sleep_ms(10);
    }
    return KERNEL_ERR_TIMEOUT;
}

static int show_cube(Scene *scene) {
    Proto3DClient *client = &scene->client;
    Proto3DShape shape = {
        .positions = scene->positions, .normals = scene->normals,
        .uvs = scene->uvs, .triangles = scene->triangles,
        .position_capacity = 24, .uv_capacity = 24, .triangle_capacity = 12,
    };
    uint32_t mesh = 0, material = 0, frame = 0;
    int rc = proto_3d_shape_box(&shape, 1.0f, 1.0f, 1.0f);
    if (rc != KERNEL_OK) return rc;
    rc = proto_3d_create_shape_mesh(client, &shape, &mesh);
    if (rc != KERNEL_OK) return rc;
    bool lit = (client->capabilities & PROTO_3D_CAP_LIGHTING) != 0;
    rc = proto_3d_create_material_ex(client, 0, 0xfd20, 255,
                                   lit ? PROTO_3D_MATERIAL_LIT : 0, &material);
    if (rc != KERNEL_OK) return rc;
    float view[16], projection[16], camera[16], model[16];
    const float eye[3] = {2.0f, 1.4f, 3.0f};
    const float target[3] = {0.0f, 0.0f, 0.0f};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    if (!proto_3d_look_at(view, eye, target, up) ||
        !proto_3d_frustum(projection, -0.133333f, 0.133333f,
                          -0.1f, 0.1f, 0.2f, 20.0f) ||
        !proto_3d_multiply(camera, projection, view)) return -1;
    proto_3d_identity(model);
    rc = proto_3d_frame_begin(client, camera, 0x0841);
    if (rc == KERNEL_OK && lit) {
        const float ambient[3] = {0.16f, 0.18f, 0.22f};
        const float color[3] = {0.9f, 0.82f, 0.7f};
        const float direction[3] = {-1.0f, -2.0f, -1.0f};
        rc = proto_3d_frame_ambient(client, ambient);
        if (rc == KERNEL_OK) rc = proto_3d_frame_directional(client, color, direction);
    }
    if (rc == KERNEL_OK) rc = proto_3d_draw(client, mesh, &material, 1, model);
    if (rc == KERNEL_OK) rc = proto_3d_frame_submit_wait(client, &frame, 100);
    if (rc == KERNEL_OK) rc = wait_presented(client, frame);
    if (rc == KERNEL_OK) {
        proto_log(LOG_WARN, "PASS SDK hello3d: OS reports PRESENTED (visual check required)");
        proto_sleep_ms(2000);
    }
    return rc;
}

int proto_loaded_main(const ProtoElfLoadMap_t *load_map,
                      const ProtoElfStartInfo_t *start_info) {
    (void)load_map;
    (void)start_info;
    Scene *scene = proto_malloc(sizeof(*scene));
    if (scene == NULL) return -1;
    memset(scene, 0, sizeof(*scene));
    int rc = proto_3d_init(&scene->client, "SDK Hello 3D", 1);
    if (rc == KERNEL_OK) rc = show_cube(scene);
    if (rc != KERNEL_OK) proto_log(LOG_ERROR, "FAIL SDK hello3d: scene or presentation refused");
    int cleanup = proto_3d_deinit(&scene->client, 2000);
    if (cleanup == KERNEL_OK) {
        proto_free(scene);
    } else {
        /* Deinit timeout preserves live client storage. Leave it for process
         * teardown rather than freeing memory the service may still borrow. */
        proto_log(LOG_WARN, "FAIL SDK hello3d: retirement pending; returning for process teardown");
    }
    return rc == KERNEL_OK && cleanup == KERNEL_OK ? 0 : -1;
}
