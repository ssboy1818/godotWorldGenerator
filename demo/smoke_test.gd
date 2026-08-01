extends SceneTree


func _initialize() -> void:
    assert(ClassDB.class_exists("WorldgenSettings"))
    assert(ClassDB.class_exists("VoronoiWorldGenerator"))
    assert(ClassDB.class_exists("VoronoiWorldData"))
    assert(ClassDB.class_exists("VoronoiWorld2D"))
    assert(ClassDB.is_parent_class("WorldgenSettings", "Resource"))
    assert(ClassDB.is_parent_class("VoronoiWorldGenerator", "RefCounted"))
    assert(ClassDB.is_parent_class("VoronoiWorldData", "RefCounted"))
    assert(ClassDB.is_parent_class("VoronoiWorld2D", "Node2D"))

    var settings := WorldgenSettings.new()
    assert(is_equal_approx(settings.edge_strength, 0.55))
    assert(settings.river_source_count == 12)
    assert(is_equal_approx(settings.river_minimum_source_elevation, 0.6))
    assert(is_equal_approx(settings.river_randomness, 0.25))
    assert(is_equal_approx(settings.river_elevation_tolerance, 0.03))
    assert(is_equal_approx(settings.province_start_score, 10.0))
    assert(is_equal_approx(settings.province_river_contribution, 5.0))
    assert(is_equal_approx(settings.province_elevation_contribution, 10.0))
    assert(is_equal_approx(settings.province_distance_contribution, 5.0))
    assert(is_equal_approx(settings.province_short_border_contribution, 5.0))
    assert(is_equal_approx(settings.province_base_cost, 1.0))
    assert(settings.province_minimum_region_count == 3)
    assert(is_equal_approx(settings.equator_temperature, 30.0))
    assert(is_equal_approx(settings.pole_temperature, -20.0))
    assert(is_equal_approx(settings.vegetation_coefficient, 1.0))
    assert(is_equal_approx(settings.humidity_coefficient, 1.0))
    settings.bounds = Rect2(0.0, 0.0, 512.0, 512.0)
    settings.seed = 93
    settings.columns = 16
    settings.rows = 16
    settings.jitter = 0.8
    settings.sea_level = 0.3
    settings.edge_decay_ratio = Vector2(0.1, 0.1)
    settings.edge_strength = 0.2
    settings.equator_temperature = 35.0
    settings.pole_temperature = -25.0
    settings.vegetation_coefficient = 1.25
    settings.humidity_coefficient = 0.8
    settings.river_source_count = 24
    settings.river_minimum_source_elevation = 0.5
    settings.river_randomness = 0.35
    settings.river_elevation_tolerance = 0.03
    settings.province_start_score = 10.0
    settings.province_river_contribution = 5.0
    settings.province_elevation_contribution = 10.0
    settings.province_distance_contribution = 5.0
    settings.province_short_border_contribution = 5.0
    settings.province_base_cost = 1.0
    settings.province_minimum_region_count = 3

    var generator := VoronoiWorldGenerator.new()
    generator.settings = settings
    var world := generator.generate() as VoronoiWorldData
    assert(world != null)
    assert(world.cell_count == 256)
    assert(world.sites.size() == world.cell_count)
    assert(world.cell_vertex_offsets.size() == world.cell_count + 1)
    assert(world.neighbor_offsets.size() == world.cell_count + 1)
    assert(world.elevations.size() == world.cell_count)
    assert(world.temperatures.size() == world.cell_count)
    assert(world.humidities.size() == world.cell_count)
    assert(world.vegetations.size() == world.cell_count)
    assert(world.region_types.size() == world.cell_count)
    assert(world.river_count > 0)
    assert(world.river_offsets.size() == world.river_count + 1)
    assert(world.river_downstream_indices.size() == world.river_count)
    assert(world.river_vertices.size() == world.river_strengths.size())
    assert(world.cell_edge_rivers.size() == world.vertices.size())
    assert(world.cell_vertex_offsets[-1] == world.vertices.size())
    assert(world.neighbor_offsets[-1] == world.neighbors.size())
    assert(world.river_offsets[-1] == world.river_vertices.size())
    assert(world.province_count > 0)
    assert(world.province_offsets.size() == world.province_count + 1)
    assert(world.province_region_ids.size() <= world.cell_count)
    assert(world.province_seed_region_ids.size() == world.province_count)
    assert(world.province_remaining_scores.size() == world.province_count)
    assert(world.region_province_indices.size() == world.cell_count)
    assert(world.province_offsets[-1] == world.province_region_ids.size())
    for region in world.cell_count:
        assert(world.temperatures[region] >= settings.pole_temperature)
        assert(world.temperatures[region] <= settings.equator_temperature)
        assert(world.humidities[region] >= 0.0)
        assert(world.humidities[region] <= 1.0)
        assert(world.vegetations[region] >= 0.0)
        assert(world.vegetations[region] <= 1.0)
    var river_region_edges := 0
    for river in world.cell_edge_rivers:
        assert(river >= -1 and river < world.river_count)
        if river >= 0:
            river_region_edges += 1
    assert(river_region_edges > 0)
    var linked_river_segments := 0
    for river in world.river_count:
        var first_node: int = world.river_offsets[river]
        var after_last_node: int = world.river_offsets[river + 1]
        assert(after_last_node - first_node >= 2)
        for node in range(first_node + 1, after_last_node):
            assert(world.river_strengths[node] >= world.river_strengths[node - 1])

        var downstream: int = world.river_downstream_indices[river]
        assert(downstream >= -1 and downstream < world.river_count)
        assert(downstream != river)
        if downstream >= 0:
            linked_river_segments += 1
            var downstream_first: int = world.river_offsets[downstream]
            assert(world.river_vertices[after_last_node - 1]
                   == world.river_vertices[downstream_first])
            assert(world.river_strengths[after_last_node - 1]
                   == world.river_strengths[downstream_first])
    assert(linked_river_segments > 0)

    var assigned_regions := PackedByteArray()
    assigned_regions.resize(world.cell_count)
    for province in world.province_count:
        var first_region: int = world.province_offsets[province]
        var after_last_region: int = world.province_offsets[province + 1]
        assert(after_last_region > first_region)
        assert(world.province_seed_region_ids[province]
               == world.province_region_ids[first_region])
        assert(world.province_remaining_scores[province] >= 0.0)
        for offset in range(first_region, after_last_region):
            var region: int = world.province_region_ids[offset]
            assert(region >= 0 and region < world.cell_count)
            assert(assigned_regions[region] == 0)
            assert(world.region_province_indices[region] == province)
            assigned_regions[region] = 1
    for region in world.cell_count:
        if world.region_types[region] == VoronoiWorldData.REGION_TYPE_LAND:
            assert(assigned_regions[region] == 1)
            assert(world.region_province_indices[region] >= 0)
        else:
            assert(assigned_regions[region] == 0)
            assert(world.region_province_indices[region] == -1)

    var repeated := generator.generate() as VoronoiWorldData
    assert(repeated != null)
    assert(repeated.sites == world.sites)
    assert(repeated.vertices == world.vertices)
    assert(repeated.elevations == world.elevations)
    assert(repeated.temperatures == world.temperatures)
    assert(repeated.humidities == world.humidities)
    assert(repeated.vegetations == world.vegetations)
    assert(repeated.region_types == world.region_types)
    assert(repeated.river_vertices == world.river_vertices)
    assert(repeated.river_strengths == world.river_strengths)
    assert(repeated.river_offsets == world.river_offsets)
    assert(repeated.river_downstream_indices == world.river_downstream_indices)
    assert(repeated.cell_edge_rivers == world.cell_edge_rivers)
    assert(repeated.province_region_ids == world.province_region_ids)
    assert(repeated.province_offsets == world.province_offsets)
    assert(repeated.province_seed_region_ids == world.province_seed_region_ids)
    assert(repeated.province_remaining_scores == world.province_remaining_scores)
    assert(repeated.region_province_indices == world.region_province_indices)

    var node := VoronoiWorld2D.new()
    node.settings = settings
    var node_world := node.generate() as VoronoiWorldData
    assert(node_world != null)
    assert(node_world.sites == world.sites)
    node.free()

    print("Worldgen GDExtension smoke test passed with ", world.cell_count, " cells.")
    quit(0)
