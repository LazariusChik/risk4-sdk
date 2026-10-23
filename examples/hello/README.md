# Hello

The smallest complete example: [hello.c](hello.c) allocates and frees 64 bytes,
logs a PASS message and returns normally through the loaded-app runtime.

From the repository root:

```sh
python tools/protelf_dev.py build-bundle --sdk-profile core --app examples/hello --app-id 7001 --name hello --package dev.proto.sdk.hello --out build/hello.pelf
```

The app draws no UI. On compatible firmware, the expected log is:

```text
PASS SDK hello: allocated, freed, returning normally
```

The message uses WARN so the development logger's ordinary INFO filter does
not hide it.
