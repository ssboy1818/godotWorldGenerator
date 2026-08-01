# Worldgen GDExtension

Worldgen is a Godot 4.4 GDExtension backed by an engine-independent C++23 core.
It synchronously generates a bounded Voronoi world with deterministic terrain
elevation, land/water classification, cell adjacency, and rivers that follow
Voronoi borders downhill from high inland vertices. Regions are also grouped into
deterministic, contiguous provinces using configurable terrain and river costs.
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
settings.land_type_snow_temperature = 0.0
settings.land_type_cold_temperature = 6.0
settings.land_type_hot_temperature = 22.0
settings.land_type_dry_humidity = 0.3
settings.land_type_wet_humidity = 0.7
settings.land_type_sparse_vegetation = 0.35
settings.land_type_lush_vegetation = 0.6
settings.land_type_lowland_elevation = 0.15
settings.land_type_hill_elevation = 0.4
settings.land_type_mountain_elevation = 0.7
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
settings.province_short_border_contribution = 5.0
settings.province_base_cost = 1.0
settings.province_minimum_region_count = 3
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

`sites`, `elevations`, `region_land_indices`, and `region_types` contain one
entry per cell. Water regions have `-1` in `region_land_indices`. Land region
`i` uses that value as an index into the compact `land_region_ids`,
`land_temperatures`, `land_humidities`, `land_vegetations`, and `land_types`
arrays; `land_region_ids` provides the inverse mapping back to the region/cell
ID.
`cell_vertex_offsets` and `neighbor_offsets` contain `cell_count + 1` entries, so
cell `i` uses the half-open ranges `[offsets[i], offsets[i + 1])` in the shared
`vertices` and `neighbors` arrays. Region type constants are available as
`VoronoiWorldData.REGION_TYPE_WATER` and `VoronoiWorldData.REGION_TYPE_LAND`.

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

Boundary-connected water is treated as ocean. Ocean-adjacent land receives the
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
Elevation is normalized above each region's effective sea level, so high land is
cooled without tying the result to an absolute world height. The centered
humidity term weakly warms dry land and cools wet land without shifting a neutral
humidity of `0.5`. Set either influence to zero to disable it. Both endpoint
temperatures must be in `[-50, 50]`, the pole cannot be warmer than the equator,
and the finalized result is clamped to that range. Noise strength, elevation
cooling, and humidity influence are in `[0, 100]`; noise frequency must be
positive, and the latitude exponent is in `(0, 10]`.

Each land region is classified after river and ocean contributions and final
temperature are applied.
Elevation is normalized from the region's effective sea level to the maximum
terrain elevation. Land-type settings define shared condition boundaries and
must satisfy `snow <= cold < hot`, `dry < wet`, `sparse < lush`, and
`lowland < hill < mountain`. Classification combines conditions in the following
priority order:

| Land type | Rule |
| --- | --- |
| `LAND_TYPE_SNOW_PEAKS` | elevation at least `mountain` and temperature at most `snow` |
| `LAND_TYPE_MOUNTAIN` | elevation at least `mountain` and temperature above `snow` |
| `LAND_TYPE_TUNDRA` | temperature at most `cold` and vegetation at most `sparse` |
| `LAND_TYPE_HILLS` | elevation at least `hill` and temperature above `cold` |
| `LAND_TYPE_SWAMP` | elevation at most `lowland`, temperature above `cold`, humidity at least `wet`, and vegetation at least `lush` |
| `LAND_TYPE_RAINFOREST` | temperature at least `hot`, humidity at least `wet`, and vegetation at least `lush` |
| `LAND_TYPE_DESERT` | temperature at least `hot`, humidity at most `dry`, and vegetation at most `sparse` |
| `LAND_TYPE_FOREST` | temperature above `cold`, humidity above `dry`, and vegetation at least `lush` |
| `LAND_TYPE_SPARSE` | humidity at most `dry` and vegetation at most `sparse` |
| `LAND_TYPE_FIELDS` | all remaining land |

`land_types` is aligned with the other compact land-only arrays. Water regions
do not have a land type and retain `-1` in `region_land_indices`.

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

Province generation runs after rivers and considers land regions only. The
lowest-ID unclaimed land region becomes a seed with `province_start_score`; its
province repeatedly claims the cheapest unclaimed land region neighboring any
current member. Crossing from region `a` to region `b` costs:

```text
province_base_cost
    + province_elevation_contribution * abs(elevation[a] - elevation[b])
    + province_distance_contribution
        * distance(site[b], site[seed]) / world_bounds_diagonal
    + province_short_border_contribution
        * clamp(1 - shared_border_length(a, b) / average_cell_length, 0, 1)
    + province_river_contribution if their shared border carries a river
```

Here `average_cell_length` is
`sqrt(world_bounds_area / cell_count)`, so the short-border term is independent
of world scale and site count. Borders at least that long add no penalty; shorter
borders add progressively more, up to the configured contribution.

The seed site is the stable center of its province, and the seed itself is free.
At each step the generator selects the globally cheapest frontier transition;
costs in the same absolute `EPS` bucket use region and source IDs as deterministic
tie-breakers. Growth stops when the frontier is empty or its cheapest claim
exceeds the remaining score, then the next unclaimed land region starts another
province.

After growth, provinces containing fewer than
`province_minimum_region_count` regions are removed when another province is
reachable through land neighbors. Their regions are reassigned independently in
neighbor-distance rounds. Each region selects the province held by the largest
number of its already assigned neighbors, with the lower province ID breaking a
tie. This can split one small province across several surrounding provinces. If
a connected land component contains only small provinces, its largest province
(then lowest ID) remains as an anchor so the component always has an owner.
Absorbed regions are appended after the surviving province's original claim
order; merging does not spend its remaining growth score. A value of `1`
disables this cleanup in practice.

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
