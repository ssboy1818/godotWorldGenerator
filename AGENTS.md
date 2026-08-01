# Worldgen Project Guide

## Project mission

Worldgen is a Godot 4 GDExtension that procedurally generates a
bounded two-dimensional world and partitions it into Voronoi cells. Each cell is a
gameplay region with geometry and generated properties such as elevation and
land/water classification, temperature, humidity, and vegetation. Godot can
request a seeded world, inspect its cells and their relationships, and use the
result to build meshes, maps, navigation, simulation, or editor previews.

The repository contains engine-independent C++ core libraries and a thin Godot 4.4
binding layer. The binding registers settings, synchronous generation, immutable
packed result data, and a `Node2D` facade. It does not yet render the generated
world or provide a complete visualization demo.

## Current generation pipeline

`VoronoiWorldGenerator::generate()` invokes the complete pipeline:

1. Convert and validate a `WorldgenSettings` resource into core settings.
2. Use `JitteredGridSiteGenerator` to place one seed point in each grid slot.
3. Pass those sites to `Fortune`, which performs a Fortune-style sweep and builds
   Voronoi topology in a `DCEL`.
4. Clip/finalize every cell against the world bounds and store its ordered polygon
   vertices in `Polygon::vertices`.
5. Sample multi-octave Perlin noise at each site's position.
6. Sample latitude-based temperature and independent, domain-separated humidity
   and vegetation noise at each site's position.
7. Raise the effective sea level near the world border using a smooth edge-decay
   mask and build one classified `Region` per polygon. Record high land regions
   as possible river sources during this pass.
8. Canonicalize shared polygon vertices into a boundary graph, route reachable
   candidates toward water with a bounded elevation tolerance, accumulate flow at
   confluences, split the network into linked river segments, and annotate the
   corresponding region edge indices.
9. Grow contiguous provinces from unclaimed land-region seeds using elevation,
   river, normalized seed-distance, and base claim costs, and assign every land
   region a province ID while leaving water regions unassigned.
10. Return an immutable core `World` containing the bounds, diagram, regions,
    rivers, and provinces.
11. Copy the result into a read-only `VoronoiWorldData` with packed Godot arrays.

The generator is deterministic when its settings are fixed. A single explicit
`WorldGenerationSettings::seed` controls jittered site placement, terrain and
climate noise, and river routing; generation does not depend on mutable global
random state.

## Repository layout and ownership

### `geometry/`

The dependency-free geometry and topology layer.

- `Vector2.h`: generic two-dimensional vector and the `Vector2d`, `Vector2f`, and
  `Vector2i` aliases.
- `BoundingBox.h`: validated axis-aligned bounds; both dimensions must be positive.
- `Id.h`: integer IDs for sites, vertices, half-edges, and polygons. `INVALID_ID`
  is `-1`, and `EPS` is the shared floating-point tolerance.
- `Site.h`, `Vertex.h`, `Edge.h`, and `Polygon.h`: records used by the diagram.
- `DCEL.h`: owns those records and provides checked ID-based access plus topology
  operations. It stores paired half-edges rather than pointer-linked objects.
- `Circle.h` and `Arc.h`: sweep-line geometry and beach-line state.

IDs are vector indices. They are stable while a `DCEL` is populated and become
invalid after `DCEL::clear()` or when referring to a different diagram. Do not
persist a bare ID without also retaining the owning world/diagram.

### `voronoi/`

Construction of the bounded Voronoi diagram.

- `Fortune` owns the event queue, beach line, and output `DCEL` for one calculation.
- `EventQueue` owns site and circle events; invalidated circle events remain owned
  until the queue is cleared and are ignored when popped.
- `BeachLine` is a red-black tree for search with an additional linked ordering of
  arcs. Any edit must preserve both structures and the `Arc*`-to-node lookup.
- `Fortune.cpp` handles sweep events, completes unbounded bisectors at the world
  boundary, and constructs bounded polygon vertex loops.

`Cell::vertices` is currently the authoritative cell outline for consumers. The
final bounding/clipping pass adds polygon vertices but does not
turn every clipped boundary segment into a fully linked DCEL half-edge cycle.
Consequently, do not assume that walking `Edge::next` visits the same complete
boundary as `Polygon::vertices` until that invariant is deliberately implemented
and tested.

The final clipping pass starts with the bounding rectangle and clips it against
neighbor half-planes. It uses neighbors found in the sweep topology plus a small
nearest-site supplement. Changes here need adversarial tests for sparse layouts,
boundary cells, nearly collinear points, coincident/near-coincident points, and
highly nonuniform site distributions.

### `noise/`

Procedural elevation functions.

- `perlinNoise` evaluates deterministic gradient noise and can normalize it.
- `fractalNoise` combines several octaves (fBm-style) and normalizes their sum.
- `edgeDecay` creates a smooth mask used to raise sea level near the box boundary.

Noise functions receive their seed explicitly. Avoid introducing global
configuration state, especially before adding concurrent generation.

### `climate/`

Deterministic region climate sampling.

- `ClimateGenerator` owns validated temperature, coefficient, seed, and shared
  noise-shape settings for one world.
- Temperature interpolates from the equator at the vertical center to equal pole
  temperatures at the top and bottom bounds.
- Humidity and vegetation use independent seed domains and normalized fBm noise;
  their coefficients scale and clamp results to `[0, 1]`.

Climate code depends only on geometry and noise and must remain independent from
terrain domain objects and Godot.

### `terrain/`

The domain-level orchestration layer.

- `SiteGenerator` is the strategy interface for point placement.
- `JitteredGridSiteGenerator` creates a regular grid with bounded random offsets.
  `columns` and `rows` must be positive; `jitter` must be in `[0, 1]`; and its seed
  must be supplied explicitly.
- `WorldGenerator` owns an immutable copy of `WorldGenerationSettings`, validates
  it, invokes site, Voronoi, noise, climate, river, and province generation, and
  assembles a `World`.
- `RiverNode` stores a polygon vertex and accumulated flow strength. `River`
  stores an ordered node vector plus the ID of its shared downstream segment.
- `World` owns the generated diagram, regions, rivers, and provinces.
- `Region` references a polygon by ID and stores elevation, land/water type,
  temperature, humidity, vegetation, an optional land-only province ID, and one
  optional river ID per ordered polygon edge.
- `Province` stores an ordered union of region IDs, its seed, and remaining claim
  score.

The dependency direction is intentional:

```text
geometry <- voronoi --------\
    ^                         >- terrain <- Godot bindings
    +-- noise <- climate ----/
```

Keep the core independent from Godot. Bindings depend on these libraries and
convert their values at the boundary; geometry, Voronoi, noise, climate, and
terrain code must not depend on Godot headers or engine object lifetimes.

### `godot/`

The GDExtension adapter and registration layer.

- `WorldgenSettings` is a `Resource` with Inspector-editable generation inputs.
- `VoronoiWorldGenerator` is a `RefCounted` synchronous generation service.
- `VoronoiWorldData` is a read-only `RefCounted` result with packed arrays.
- `VoronoiWorld2D` is a scene-instantiable `Node2D` facade over the service.
- `RegisterTypes.cpp` exports `worldgen_library_init` and registers all four
  classes at scene initialization level.

Godot-facing code may depend on the core, but the reverse dependency is forbidden.

### Root files

- `CMakeLists.txt` builds the core layers, `godot-cpp`, and the shared
  `worldgen_extension` target.
- `demo/addons/worldgen/worldgen.gdextension` is the Godot 4.4 extension manifest.
- `demo/project.godot` is a minimal project that loads the extension.
- Build trees, IDE metadata, and compiled outputs are generated
  artifacts and must not be committed.

## Build and verification

The current project requires CMake 3.17+, a C++23 compiler/standard library, and
the `godot-cpp` submodule on its 4.4 branch.

Configure and build out of source:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DGODOTCPP_TARGET=template_debug
cmake --build build --parallel
```

The debug library is written to `demo/addons/worldgen/bin/` and is loaded by the
manifest when the `demo/` project is opened in a compatible Godot editor.

For an optimized performance check:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
    -DGODOTCPP_TARGET=template_release
cmake --build build-release --parallel
```

`demo/smoke_test.gd` provides an engine-level registration, packed-data, and
determinism smoke test. Focused C++ test targets cover current generation behavior,
but there is not yet automated CI; continue adding coverage rather than treating a
successful shared-library build as sufficient behavioral validation.

When changing generation code, verify at least:

- the project builds in Debug and Release;
- fixed seeds produce repeatable sites, polygons, elevations, and classifications;
- every returned site lies inside the requested bounds;
- every returned polygon has at least three finite, in-bounds vertices;
- each region references a valid polygon, and each polygon references a valid site;
- cells do not have visible gaps or overlap except at shared boundaries;
- invalid bounds/settings fail clearly instead of producing corrupt geometry;
- generation time and memory remain reasonable for large grids.

Prefer small deterministic fixtures in unit tests. Compare coordinates using an
explicit tolerance, not exact floating-point equality. For topology tests, check
structural invariants separately from approximate geometric values.

## Target Godot architecture

Implement the extension as a thin adapter around the existing core rather than
rewriting the algorithms in Godot-specific types.

Recommended layers:

1. **Core libraries:** the existing `geometry`, `voronoi`, `noise`, `climate`, and
   `terrain` targets, with no Godot dependency.
2. **GDExtension bindings:** registration/initialization plus Godot-facing classes
   that translate settings and generated results. This initial layer exists.
3. **Packaging:** the native library, `.gdextension` file, platform-specific build
   outputs, and any editor plugin metadata. The descriptor and local build layout
   exist; supported-platform release packaging remains future work.
4. **Demo/tests:** the minimal Godot project currently loads the extension but does
   not yet draw cell polygons or provide engine-level automated tests.

The initial Godot API exposes `WorldgenSettings`, `VoronoiWorldGenerator`,
`VoronoiWorldData`, and `VoronoiWorld2D`. It covers:

- bounds or world size;
- site columns/rows or another site-placement strategy;
- jitter and all random seeds;
- sea level, edge-decay ratio/strength, noise parameters, climate coefficients and
  temperatures, and river source/routing controls;
- per-cell site position, ordered polygon vertices, elevation, temperature,
  humidity, vegetation, and land/water type;
- stable cell indices and neighboring cell indices;
- ordered river vertices, per-node strengths, river offsets, and downstream
  segment indices;
- per-cell river IDs aligned with ordered polygon edges;
- clear validation errors suitable for both GDScript and C++ callers.

Use Godot containers at the binding boundary (`PackedVector2Array`, packed numeric
arrays, `Array`, or typed `Dictionary`/resource data as appropriate), but keep
native storage compact in the core. Convert `Vector2d` to Godot `Vector2` explicitly
and account for Godot builds where `real_t` is `float`. Do not leak references or
pointers into vectors owned by temporary `World` objects.

Generation may later run on a worker thread, but only the pure core calculation
belongs there. Creation or mutation of Godot `Object`, `Resource`, scene, rendering,
or editor state must follow Godot's threading rules and normally occur on the main
thread. Explicit immutable settings and per-run seeds are prerequisites for safe
parallel generation.

## Roadmap and definition of done

Work should normally progress in this order unless a task has narrower scope:

1. Add deterministic unit tests for vectors/bounds, noise, site generation, DCEL
   invariants, bounded Voronoi cells, and region classification.
2. Remove hidden global configuration, beginning with `noise::seed`, and put all
   reproducibility controls in `WorldGenerationSettings` or nested settings.
3. Define cell adjacency as supported output and make polygon/DCEL boundary
   invariants explicit.
4. Add `godot-cpp` and a shared-library GDExtension target without coupling the
   core libraries to Godot.
5. Bind generator settings and immutable world/cell data to Godot 4.
6. Package a `.gdextension` descriptor and provide a small visualization demo.
7. Add engine-level smoke tests, documentation, supported-platform builds, and
   release packaging.

The project reaches its primary goal when a Godot 4 project can install the
extension, generate the same bounded world from the same settings and seed, access
all cells and their neighbors from GDScript, and render/use the result without the
standalone SVG exporter.

## Engineering rules for contributors and agents

- Preserve the core/Godot dependency boundary described above.
- Keep project-owned public symbols in the `worldgen` namespace; noise utilities
  live in the nested `worldgen::noise` namespace.
- Read the relevant public headers and their implementation before changing a
  subsystem; several invariants span `geometry` and `voronoi`.
- Keep public inputs validated. Reject non-finite values, invalid ranges, invalid
  IDs, and inconsistent bounds close to the API boundary.
- Favor ownership by value and IDs over long-lived pointers. Raw pointers inside
  the beach line/event system are tightly scoped to a calculation and require
  careful invalidation.
- Preserve deterministic behavior. Never seed from `random_device` when the caller
  has supplied a seed, and never make iteration order an accidental source of
  nondeterminism.
- Treat numerical degeneracies as normal inputs to handle or reject explicitly.
  Do not hide them with arbitrary large epsilons.
- Use `double` in the core unless a measured requirement justifies changing it.
- Follow the existing C++ style: four-space indentation, braces on the same line,
  `PascalCase` types, `camelCase` functions, and `m_` data members. Public headers
  use `#pragma once`; declarations live in `include/` and nontrivial definitions in
  `src/`.
- Prefer `[[nodiscard]]` for meaningful returned values and `noexcept` only when it
  is correct for the whole implementation.
- Add source files to the owning subproject's `CMakeLists.txt`; expose dependencies
  through the existing `worldgen::<layer>` aliases.
- Add or update tests with behavioral changes. Visual inspection is useful for
  diagnosis but is not a replacement for assertions.
- Measure performance changes using fixed inputs and Release builds. Avoid
  optimizing at the cost of unverified geometry correctness.
- Do not edit generated build trees, native libraries, or `.qtcreator` user files.
- Preserve unrelated user changes in a dirty working tree.

## Known gaps

Do not describe the following as completed:

- comprehensive automated tests and CI;
- a complete linked DCEL boundary for every clipped polygon;
- documented handling of duplicate sites and all geometric degeneracies;
- chunking, streaming, level of detail, erosion, advanced climate simulation,
  biomes, full hydrology such as basin-wide runoff, lakes, and floodplains, roads,
  settlements, or serialization;
- stable performance/memory guarantees for production-sized worlds;
- a polygon-rendering Godot demo and supported-platform release packages.

Those features may be added incrementally, but the essential product remains a
reliable, deterministic Godot extension for worlds partitioned into bounded Voronoi
cells.
