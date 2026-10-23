# Hello 3D

Build a procedural orange cube, upload its mesh and material, submit one frame,
wait for the OS to report `PRESENTED`, and close the surface after two seconds.
Lighting is enabled only when the runtime advertises `PROTO_3D_CAP_LIGHTING`;
otherwise the same geometry uses an unlit material. No model files are needed.

```sh
python tools/protelf_dev.py build-bundle --sdk-profile full --app examples/hello3d --app-id 7003 --name hello3d --package dev.proto.sdk.hello3d --heap 65536 --out build/hello3d.pelf
```

Read [hello3d.c](hello3d.c) alongside the [Game3D guide](../../docs/game3d.md).
The app allocates scene/client state on the process heap, bounds submission and
presentation waits, and waits for resource retirement before freeing storage.
If retirement times out, normal process teardown owns the remaining cleanup.
