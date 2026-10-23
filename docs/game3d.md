# Game3D

RISK4 includes a software 3D engine for ESP32-P4. Apps upload meshes, textures
and immutable materials through mediated handles, then submit scene instances
and query frame completion. The renderer lives in the closed OS; this SDK
includes public headers and a compiled app-side library.

## Implemented interfaces

| Area | Available functionality |
| :--- | :--- |
| Rendering | Depth buffering, perspective-correct textures, clipping, instancing and back-face culling |
| Materials | RGB565 and indexed textures, uploaded mipmaps, vertex colors, lit materials, dithered opacity and blending |
| Camera and lighting | Perspective and orthographic cameras, ambient/directional/point lighting, linear fog |
| Presentation | 320x240 scene output, optional 240x180 or 160x120 scene rendering expanded to 320x240; HUD stays at full resolution |
| App helpers | Procedural shapes, matrix/quaternion math, billboards and rigid node animation |
| Scene queries | Ray casting and bounds queries, sphere/capsule collision and move-and-slide helpers |
| Asset runtime | `.g3a` loading and resource/preload helpers |

Use `proto_3d_query_caps()` or the initialized client's capabilities before
enabling optional features. Constants in a header do not override the running
firmware's reported capability mask. Rigid node animation does not provide
skinned meshes; collision helpers do not provide a general rigid-body simulator.

## Start with a cube

[Hello 3D](../examples/hello3d/README.md) uses `proto_loaded.h` and `proto_3d.h`.
Build with `--sdk-profile full`, or run `python build_examples.py` to build all
the examples. The `core` profile remains available for apps needing only the
loaded runtime and Game2D helpers.

The app library is supplied as `lib/libproto_full.a` for RV32 / ilp32f.
Include `proto_3d.h` for graphics and `proto_loaded.h` for runtime calls.
Implementation sources and internal headers remain private.

The project's glTF/GLB-to-`.g3a` host conversion pipeline is not packaged in
this snapshot. The included sample uses procedural geometry. No game code,
models, textures, private renderer or board drivers are included.

## Ownership and frame lifecycle

Zero-initialize a client before `proto_3d_init()`. Serialize access if several
app threads use one client. Upload arrays are copied synchronously; resource
handles are immutable until destroyed. Keep live SDK storage until retirement.

Build a frame with `proto_3d_frame_begin()`, add lights/draws, then submit it.
`proto_3d_frame_submit_wait()` can wait at most 100 ms for queue space. A `BUSY`
return preserves the built frame for retry. Acceptance, completion and
presentation are distinct states; use `proto_3d_query_frame()` to observe them.
Check errors and cancelled/rejected frames instead of spinning indefinitely.

`proto_3d_deinit()` cancels/closes and waits for retirement. A timeout preserves
the client so it can be retried; it does not authorize freeing borrowed memory.

## Current protocol bounds

Protocol version **1** currently allows up to 64 instances, 1,024 positions and
triangles per mesh, 3,072 positions and triangles per frame, 64 resources and
eight lights. Textures are at most 256 pixels per side. The resource budget is
512 KiB and transient budget 1 MiB. These are admission limits, not a promise
that every maximum fits simultaneously or meets a particular frame rate.
Query the running kernel and handle quota/allocation failures.

The renderer uses both CPU cores for tile work; this is software rendering,
with no OpenGL/GPU claim. Frame rate depends on scene, material, render scale,
firmware, clocks and board. This SDK snapshot supplies build evidence, no new
device benchmark. Add real gameplay to the [3D recording slot](media.md).
