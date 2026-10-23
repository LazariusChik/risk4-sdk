# Hello 2D

[hello2d.c](hello2d.c) allocates a `ProtoGfx_t`, opens a Game2D surface, submits
a rectangle and text, waits two seconds after acceptance and releases the
surface/buffers through SDK cleanup.

From the repository root:

```sh
python tools/protelf_dev.py build-bundle --sdk-profile core --app examples/hello2d --app-id 7002 --name hello2d --package dev.proto.sdk.hello2d --heap 16384 --out build/hello2d.pelf
```

The logical game surface is 320x240. The OS handles physical scanout and any
user-selected sharp rendering mode. This example uses no textures, external
assets or 3D helpers.

A dropped or refused submission is reported as failure.
