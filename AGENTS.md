# Worldgen Project Guide

## Project mission

Worldgen is intended to become a Godot 4 GDExtension that procedurally generates a
bounded two-dimensional world and partitions it into Voronoi cells. Each cell is a
gameplay region with geometry and generated properties such as elevation and
land/water classification. Godot should eventually be able to request a seeded
world, inspect its cells and their relationships, and use the result to build
meshes, maps, navigation, simulation, or editor previews.

The repository is currently the engine-independent C++ prototype of that system.
It can generate a world and write an SVG preview, but it does **not** yet contain
`godot-cpp`, GDExtension entry points, Godot classes, an extension manifest, or a
Godot demo project. Keep this distinction explicit in documentation and code
reviews: Godot integration is the primary goal, not an existing feature.

## Current generation pipeline

The executable in `main.cpp` demonstrates the complete pipeline that exists now:

1. Construct a `BoundingBox` for the finite world.
2. Use `JitteredGridSiteGenerator` to place one seed point in each grid slot.
3. Pass those sites to `Fortune`, which performs a Fortune-style sweep and builds
   Voronoi topology in a `DCEL`.
4. Clip/finalize every cell against the world bounds and store its ordered polygon
   vertices in `Polygon::vertices`.
5. Sample multi-octave Perlin noise at each site's position. Apply edge decay so
   elevation approaches zero near the world border.
6. Build one `Region` per polygon and classify it as land or water according to
   `WorldGenerationSettings::seaLevel`.
7. Return an immutable `World` containing the bounds, diagram, and regions.
8. The prototype executable writes those regions to `voronoi.svg` and, in debug
   builds, prints timing and geometry counts.

The generator is deterministic when its seeds and settings are fixed. The site
generator owns a seed, while terrain noise currently uses the separate global
`noise::seed`; consolidating both into explicit world settings is required before
the public Godot API is considered stable.

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

`Polygon::vertices` is currently the authoritative cell outline for consumers and
SVG rendering. The final bounding/clipping pass adds polygon vertices but does not
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
- `noise` combines several octaves (fBm-style) and applies the edge-decay mask.
- `edgeDecay` creates island-like boundaries by lowering elevation near the box.

`noise::seed` is mutable global state. Avoid introducing additional global state;
replace this seed with explicit generator/configuration state when evolving the
API, especially before adding concurrent generation.

### `terrain/`

The domain-level orchestration layer.

- `SiteGenerator` is the strategy interface for point placement.
- `JitteredGridSiteGenerator` creates a regular grid with bounded random offsets.
  `columns` and `rows` must be positive; `jitter` must be in `[0, 1]`.
- `WorldGenerator` validates settings, invokes Voronoi and noise generation, and
  assembles a `World`.
- `World` owns the generated diagram and regions.
- `Region` references a polygon by ID and stores elevation plus land/water type.

The dependency direction is intentional:

```text
geometry <- voronoi ----\
    ^                     >- terrain <- prototype executable
    +------- noise ------/
```

Keep the core independent from Godot. Future bindings should depend on these
libraries and convert their values at the boundary; geometry, Voronoi, noise, and
terrain code must not depend on Godot headers or engine object lifetimes.

### Root files

- `CMakeLists.txt` builds the four layers and the `worldgen` prototype executable.
- `main.cpp` is a smoke-test/demo program and SVG exporter, not the eventual Godot
  entry point.
- `voronoi.svg`, build trees, IDE metadata, and compiled outputs are generated
  artifacts and must not be committed.

## Build and verification

The current project requires CMake 3.16+ and a C++23 compiler/standard library.
The debug executable uses `<print>`, so the standard library must implement that
C++23 header.

Configure and build out of source:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Run the prototype from the directory in which `voronoi.svg` should be created:

```sh
./build/worldgen
```

For an optimized performance check:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
./build-release/worldgen
```

There is currently no automated test target. Adding focused tests is higher
priority than treating the generated SVG as sufficient validation. Once tests
exist, register them with CTest and run `ctest --test-dir build --output-on-failure`.

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

1. **Core libraries:** the existing `geometry`, `voronoi`, `noise`, and `terrain`
   targets, with no Godot dependency.
2. **GDExtension bindings:** registration/initialization plus Godot-facing classes
   that translate settings and generated results.
3. **Packaging:** the native library, `.gdextension` file, platform-specific build
   outputs, and any editor plugin metadata.
4. **Demo/tests:** a minimal Godot project that requests a deterministic world and
   draws all cell polygons, plus C++ unit/integration tests.

A practical initial Godot API should expose a generator object and a read-only
result object. The exact class names are not fixed yet, but the API must cover:

- bounds or world size;
- site columns/rows or another site-placement strategy;
- jitter and all random seeds;
- sea level, edge-decay ratio, and noise parameters;
- per-cell site position, ordered polygon vertices, elevation, and land/water type;
- stable cell indices and neighboring cell indices;
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
- Add or update tests with behavioral changes. A visual SVG check is useful for
  diagnosis but is not a replacement for assertions.
- Measure performance changes using fixed inputs and Release builds. Avoid
  optimizing at the cost of unverified geometry correctness.
- Do not edit generated build trees, SVG output, or `.qtcreator` user files.
- Preserve unrelated user changes in a dirty working tree.

## Known gaps

Do not describe the following as completed:

- Godot/GDExtension integration and public engine-facing API;
- automated tests and CI;
- an explicit noise seed inside world settings;
- exported cell adjacency as domain data;
- a complete linked DCEL boundary for every clipped polygon;
- documented handling of duplicate sites and all geometric degeneracies;
- chunking, streaming, level of detail, erosion, climate, biomes, rivers, roads,
  settlements, or serialization;
- stable performance/memory guarantees for production-sized worlds.

Those features may be added incrementally, but the essential product remains a
reliable, deterministic Godot extension for worlds partitioned into bounded Voronoi
cells.
