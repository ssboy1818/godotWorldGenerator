# Worldgen GDExtension

Worldgen is a Godot 4.4 GDExtension backed by an engine-independent C++23 core.
It synchronously or asynchronously generates a bounded Voronoi world with
deterministic terrain elevation, land/sea/lake classification, cell adjacency,
and rivers that follow Voronoi borders downhill from high inland vertices.
Regions are also grouped into deterministic, contiguous provinces using
configurable terrain and river costs.
An independent climate layer adds seeded temperature, humidity, and vegetation
fields, then adjusts final land temperature for latitude, elevation, and humidity.

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

You can also give CMake the editor path and use the provided run targets:

```sh
cmake -S . -B build \
    -DWORLDGEN_GODOT_EXECUTABLE=/path/to/godot \
    -DGODOTCPP_TARGET=template_debug
cmake --build build --target worldgen_editor
cmake --build build --target worldgen_smoke
```

The project itself builds a shared library, not an executable. In Qt Creator, set
the Custom Executable run configuration to the Godot 4.4 executable, use
`--editor --path %{sourceDir}/demo` as its arguments, and select `demo` as the
working directory. The `worldgen_editor` CMake target provides the same launch
without an IDE run configuration.

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
  `generate()` and worker-pool-backed `generate_async()` methods.
- `VoronoiWorldGenerationTask : RefCounted` represents one asynchronous request
  and exposes completion signals, status, result, and error data.
- `VoronoiWorldData : RefCounted` exposes immutable packed result buffers.
- `VoronoiWorld2D : Node2D` provides a scene-instantiable facade with the same
  settings property and generation methods. It does not render cells automatically.

Example GDScript:

```gdscript
var generator := VoronoiWorldGenerator.new()
var settings := WorldgenSettings.new()
settings.bounds = Rect2(0, 0, 2048, 2048)
settings.seed = 42
settings.columns = 64
settings.rows = 64
settings.equator_temperature = 30.0
settings.pole_temperature = -20.0
settings.vegetation_coefficient = 1.0
settings.humidity_coefficient = 1.0
settings.temperature_noise_strength = 8.0
settings.temperature_noise_frequency = 0.003
settings.temperature_elevation_cooling = 20.0
settings.temperature_humidity_influence = 4.0
settings.temperature_latitude_exponent = 1.0
settings.ocean_humidity_coefficient = 0.2
settings.ocean_humidity_distance_ratio = 0.12
settings.land_type_polar_temperature = 0.0
settings.land_type_cold_temperature = 6.0
settings.land_type_hot_temperature = 20.0
settings.land_type_dry_humidity = 0.45
settings.land_type_wet_humidity = 0.62
settings.land_type_sparse_vegetation = 0.45
settings.land_type_lush_vegetation = 0.54
settings.land_type_wetland_elevation = 0.18
settings.landform_hill_elevation = 0.38
settings.landform_mountain_elevation = 0.68
settings.river_source_count = 12
settings.river_minimum_source_elevation = 0.6
settings.river_randomness = 0.25
settings.river_elevation_tolerance = 0.03
settings.river_humidity_coefficient = 0.05
settings.river_vegetation_coefficient = 0.05
settings.province_start_score = 10.0
settings.province_river_contribution = 5.0
settings.province_elevation_contribution = 10.0
settings.province_distance_contribution = 5.0
settings.province_land_type_contribution = 5.0
settings.province_short_border_contribution = 5.0
settings.province_base_cost = 1.0
settings.province_seed_minimum_distance = 4.0
settings.province_minimum_region_count = 3
settings.province_maximum_region_count = 0 # unlimited
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

For generation without blocking the main thread, await the request's `finished`
signal and then inspect its outcome:

```gdscript
var task: VoronoiWorldGenerationTask = generator.generate_async()
await task.finished

if not task.successful:
    push_error(task.error_message)
    return

var world: VoronoiWorldData = task.result
```

`generate_async()` snapshots and validates the current settings before submitting
the pure C++ calculation to Godot's worker pool. Multiple requests may run at the
same time and later edits to the settings resource do not affect requests already
submitted. Packed Godot arrays are created on the main thread after the core
calculation completes. A task moves through `STATUS_PENDING`, `STATUS_RUNNING`,
and either `STATUS_SUCCEEDED` or `STATUS_FAILED`; `completed(result)` or
`failed(error_message)` is emitted before the parameterless `finished` signal.
Use `done` for a polling-style completion check.

`sites`, `elevations`, `region_land_indices`, and `region_types` contain one
entry per cell. Water regions have `-1` in `region_land_indices`. Land region
`i` uses that value as an index into the compact `land_region_ids`,
`land_temperatures`, `land_humidities`, `land_vegetations`, `land_types`, and
`landforms`
arrays; `land_region_ids` provides the inverse mapping back to the region/cell
ID.
`cell_vertex_offsets` and `neighbor_offsets` contain `cell_count + 1` entries, so
cell `i` uses the half-open ranges `[offsets[i], offsets[i + 1])` in the shared
`vertices` and `neighbors` arrays. Region type constants are available as
`VoronoiWorldData.REGION_TYPE_SEA`, `VoronoiWorldData.REGION_TYPE_LAKE`, and
`VoronoiWorldData.REGION_TYPE_LAND`. A sea is boundary-connected water; every
other water component is a lake.

Climate values are sampled only for land cells at their site positions, the same
positions used for elevation. Humidity and vegetation use independent
deterministic fBm noise domains derived from the world seed and the shared noise
shape settings. Their coefficients are in `[0, 2]`; results are clamped to
`[0, 1]`. After rivers are generated, a land region touching one or more river
edges receives an additional humidity and vegetation contribution from its
strongest adjoining river segment. The additions are `river strength *
river_humidity_coefficient` and `river strength *
river_vegetation_coefficient`, respectively, and are also clamped to `[0, 1]`.
Both river climate coefficients are in `[0, 1]`; setting either one to zero
disables that contribution.

Boundary-connected water is classified as sea. Sea-adjacent land receives the
full `ocean_humidity_coefficient`, and the contribution decays smoothly to zero
over `ocean_humidity_distance_ratio * world bounds diagonal`. Only humidity is
changed. A zero coefficient or distance ratio disables ocean humidity. Inland
water that is not connected to the world boundary does not contribute.

Temperature is finalized after both river and ocean humidity have been applied:

```text
latitude = abs(y - world_center_y) / world_half_height
base = lerp(equator_temperature, pole_temperature,
            pow(latitude, temperature_latitude_exponent))
temperature = clamp(base
                    + centered_temperature_noise * temperature_noise_strength
                    - normalized_elevation * temperature_elevation_cooling
                    + (0.5 - humidity) * temperature_humidity_influence,
                    -50, 50)
```

Temperature noise has its own deterministic seed domain and
`temperature_noise_frequency`; it reuses the common octave, lacunarity, and
persistence settings. A strength of zero restores a smooth latitude curve.
Elevation uses actual generated relief: each land region's height above its local
effective sea level is divided by the greatest land relief in that world. This
makes the highest land `1`, keeps coast-level land near `0`, and avoids comparing
ordinary generated terrain with an unreachable theoretical elevation of `1`.
The same normalized value drives temperature cooling and landform classification.
The centered humidity term weakly warms dry land and cools wet land without
shifting a neutral humidity of `0.5`. Set either influence to zero to disable it.
Both endpoint temperatures must be in `[-50, 50]`, the pole cannot be warmer than
the equator, and the finalized result is clamped to that range. Noise strength,
elevation cooling, and humidity influence are in `[0, 100]`; noise frequency must
be positive, and the latitude exponent is in `(0, 10]`.

Each land region receives a climate biome after river and ocean contributions and
final temperature are applied. Temperature first selects a polar/cool,
temperate, or tropical band; humidity and vegetation select a biome only inside
that band. This gives latitude a strong influence while the low-frequency climate
fields keep neighboring regions grouped. Land-type settings must satisfy
`polar < cold < hot`, `dry < wet`, and `sparse < lush`.

| Land type | Rule |
| --- | --- |
| `LAND_TYPE_TUNDRA` | polar land, or cool land without enough moisture and vegetation for boreal forest |
| `LAND_TYPE_BOREAL_FOREST` | cool, non-dry, lush land |
| `LAND_TYPE_WETLAND` | temperate, wet, lush land no higher than `wetland_elevation` |
| `LAND_TYPE_STEPPE` | temperate land that is dry or sparsely vegetated |
| `LAND_TYPE_TEMPERATE_FOREST` | temperate, non-dry, lush land |
| `LAND_TYPE_GRASSLAND` | remaining temperate land |
| `LAND_TYPE_DESERT` | tropical land that is dry or sparsely vegetated |
| `LAND_TYPE_RAINFOREST` | tropical, wet, lush land |
| `LAND_TYPE_TROPICAL_FOREST` | tropical, non-dry, lush land that is not rainforest |
| `LAND_TYPE_SAVANNA` | remaining tropical land |

Mountains and hills are landforms, not land types. `LANDFORM_PLAIN`,
`LANDFORM_HILL`, and `LANDFORM_MOUNTAIN` are classified independently from
normalized relief using `landform_hill_elevation` and
`landform_mountain_elevation`. `land_types` and `landforms` are aligned with the
other compact land-only arrays. Water regions have neither and retain `-1` in
`region_land_indices`.

River `i` occupies `[river_offsets[i], river_offsets[i + 1])` in the flattened
`river_vertices` and `river_strengths` arrays. Each source contributes unit flow;
strengths are added where tributaries meet. `river_downstream_indices[i]` points
to the shared segment below the confluence, or is `-1` at a mouth. This stores a
merged downstream channel once while allowing a renderer to vary width by flow.
`river_source_count` limits selected headwaters; the resulting `river_count` is
the number of segments and may differ because the network is split at confluences.
Sources are sampled from high land regions, and only candidates with a route to
water are retained. A routing step may rise by at most
`river_elevation_tolerance`, which helps rivers escape shallow local depressions.

`cell_edge_rivers` parallels the flattened `vertices` array. Entry `j` identifies
the river on the polygon edge from `vertices[j]` to the next vertex of that cell,
or is `-1`. Use `cell_vertex_offsets` to find each cell's edge range.

Province generation runs after rivers and considers land regions only. It first
selects all province seeds with deterministic farthest-point sampling on the
land-neighbor graph. Every disconnected land component receives a seed. The
first seed in each component is chosen with a domain-separated hash of the world
seed and region ID; each later seed is the region farthest from its nearest
existing seed. Equal distances use the lower region ID.

Seed distance is measured along land edges rather than directly through water.
Crossing an edge from region `a` to region `b` contributes:

```text
distance(site[a], site[b]) / average_cell_length
    + province_distance_contribution
        * distance(site[a], site[b]) / world_bounds_diagonal
    + province_elevation_contribution * abs(elevation[a] - elevation[b])
    + province_land_type_contribution if land_type[a] != land_type[b]
    + province_short_border_contribution
        * clamp(1 - shared_border_length(a, b) / average_cell_length, 0, 1)
    + province_river_contribution if their shared border carries a river
```

Seeds are added until no land region is farther than
`province_seed_minimum_distance`. Smaller values create denser seeds; the
default is `4.0` graph-cost units. On flat terrain without penalties, one unit
is approximately one average cell width; terrain penalties make barriers count
as additional distance. The generator also selects enough seeds to cover the
theoretical capacity implied by `province_start_score`, `province_base_cost`,
and `province_maximum_region_count`.

After all seeds in a round are reserved, their provinces grow concurrently.
Crossing from region `a` to region `b` spends:

```text
province_base_cost
    + province_elevation_contribution * abs(elevation[a] - elevation[b])
    + province_distance_contribution
        * distance(site[b], site[seed]) / world_bounds_diagonal
    + province_land_type_contribution if land_type[b] != land_type[seed]
    + province_short_border_contribution
        * clamp(1 - shared_border_length(a, b) / average_cell_length, 0, 1)
    + province_river_contribution if their shared border carries a river
```

Here `average_cell_length` is
`sqrt(world_bounds_area / cell_count)`, so the short-border term is independent
of world scale and site count. Borders at least that long add no penalty; shorter
borders add progressively more, up to the configured contribution.

The seed site is the stable center of its province, and the seed itself is free.
The global frontier is ordered by accumulated path cost from each seed, then by
local claim cost, target region, seed region, and source region. This produces a
multi-source wavefront while retaining deterministic ties. Local claim costs are
deducted from each province's `province_start_score`; unaffordable claims are
discarded independently. A province also stops growing when it reaches
`province_maximum_region_count`; zero disables this limit. If budget or barriers
leave land unassigned, another farthest-point seed round is run on those regions.

After growth, provinces containing fewer than
`province_minimum_region_count` regions are removed when another province is
reachable through land neighbors. Their regions are reassigned independently in
neighbor-distance rounds. Each region evaluates the full claim cost through each
already assigned neighbor and selects the cheapest reachable province using that
province's original seed. Lower province and source-region IDs break an
`EPS`-bucket tie. This can split one small province across several surrounding
provinces. If
a connected land component contains only small provinces, its largest province
(then lowest ID) remains as an anchor so the component always has an owner.
Absorbed regions are appended after the surviving province's original claim
order; merging does not spend its remaining growth score. During reassignment,
targets below `province_maximum_region_count` are preferred even when their claim
cost is higher. If every adjacent target is already full, the cheapest target
still receives the region so cleanup always completes. A value of `1` disables
this cleanup in practice.

Thus every land region belongs to exactly one province, while water regions
belong to none; province indices are compact and stable for fixed settings and
generation seed.

Province `i` owns the region IDs in
`[province_offsets[i], province_offsets[i + 1])` of `province_region_ids`, in
its original claim order with the seed first, followed by any absorbed regions.
`province_seed_region_ids` and
`province_remaining_scores` contain one value per province, while
`region_province_indices[region_id]` provides the inverse lookup and is `-1` for
water regions. Consequently, `province_region_ids` contains exactly the land
region IDs rather than one entry per world region.
