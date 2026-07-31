# Worldgen GDExtension

Worldgen is a Godot 4.4 GDExtension backed by an engine-independent C++23 core.
It synchronously generates a bounded Voronoi world with deterministic terrain
elevation, land/water classification, and cell adjacency.

## Build

Initialize the Godot 4.4 C++ bindings and build a debug extension:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DGODOTCPP_TARGET=template_debug
cmake --build build --parallel
```

The library is written to `demo/addons/worldgen/bin/`, beside the package's
`worldgen.gdextension` descriptor. Open `demo/project.godot` with Godot 4.4 to
load it.

With a Godot 4.4 executable available, run the engine-level smoke test with:

```sh
godot --headless --path demo --script res://smoke_test.gd
```

For a release build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
    -DGODOTCPP_TARGET=template_release
cmake --build build-release --parallel
```

## Godot API

- `WorldgenSettings : Resource` contains Inspector-editable bounds, site-grid,
  terrain, seed, and noise settings.
- `VoronoiWorldGenerator : RefCounted` owns settings and exposes synchronous
  `generate()`.
- `VoronoiWorldData : RefCounted` exposes immutable packed result buffers.
- `VoronoiWorld2D : Node2D` provides a scene-instantiable facade with the same
  settings property and synchronous `generate()` method. It does not render cells
  automatically.

Example GDScript:

```gdscript
var generator := VoronoiWorldGenerator.new()
var settings := WorldgenSettings.new()
settings.bounds = Rect2(0, 0, 2048, 2048)
settings.seed = 42
settings.columns = 64
settings.rows = 64
generator.settings = settings

var world: VoronoiWorldData = generator.generate()
if world == null:
    return

for cell in world.cell_count:
    var first_vertex := world.cell_vertex_offsets[cell]
    var after_last_vertex := world.cell_vertex_offsets[cell + 1]
    var first_neighbor := world.neighbor_offsets[cell]
    var after_last_neighbor := world.neighbor_offsets[cell + 1]

    var polygon := world.vertices.slice(first_vertex, after_last_vertex)
    var cell_neighbors := world.neighbors.slice(first_neighbor, after_last_neighbor)
```

`sites`, `elevations`, and `region_types` contain one entry per cell.
`cell_vertex_offsets` and `neighbor_offsets` contain `cell_count + 1` entries, so
cell `i` uses the half-open ranges `[offsets[i], offsets[i + 1])` in the shared
`vertices` and `neighbors` arrays. Region type constants are available as
`VoronoiWorldData.REGION_TYPE_WATER` and `VoronoiWorldData.REGION_TYPE_LAND`.
