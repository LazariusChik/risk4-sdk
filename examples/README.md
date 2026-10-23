# Examples

The examples use the same SDK and packaging path as an ordinary app.
Build all four from the repository root:

```sh
python build_examples.py
```

| Example | Learn | Expected device behavior |
| :--- | :--- | :--- |
| [Hello](hello/README.md) | Entry point, allocation, logging, normal return | A WARN-level PASS log and normal exit |
| [Hello 2D](hello2d/README.md) | Surface ownership, a rectangle/text frame, cleanup | A blue card with white text, then return to the shell |
| [Hello 3D](hello3d/README.md) | Procedural geometry, materials, capability-gated lighting and retirement | An orange cube, then return to the shell |
| [Pentest](pentest/README.md) | Buffer validation, stale IDs and privilege faults | Denied requests return errors; the final probe should terminate this app |

See the [build guide](../docs/build.md) for installation instructions.
