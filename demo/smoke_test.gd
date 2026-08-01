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
    settings.bounds = Rect2(0.0, 0.0, 512.0, 512.0)
    settings.seed = 93
    settings.columns = 16
    settings.rows = 16
    settings.jitter = 0.8
    settings.sea_level = 0.3
    settings.edge_decay_ratio = Vector2(0.1, 0.1)
    settings.edge_strength = 0.2
    settings.river_source_count = 24
    settings.river_minimum_source_elevation = 0.5
    settings.river_randomness = 0.35
    settings.river_elevation_tolerance = 0.03

    var generator := VoronoiWorldGenerator.new()
    generator.settings = settings
    var world := generator.generate() as VoronoiWorldData
    assert(world != null)
    assert(world.cell_count == 256)
    assert(world.sites.size() == world.cell_count)
    assert(world.cell_vertex_offsets.size() == world.cell_count + 1)
    assert(world.neighbor_offsets.size() == world.cell_count + 1)
    assert(world.elevations.size() == world.cell_count)
    assert(world.region_types.size() == world.cell_count)
    assert(world.river_count > 0)
    assert(world.river_offsets.size() == world.river_count + 1)
    assert(world.river_downstream_indices.size() == world.river_count)
    assert(world.river_vertices.size() == world.river_strengths.size())
    assert(world.cell_edge_rivers.size() == world.vertices.size())
    assert(world.cell_vertex_offsets[-1] == world.vertices.size())
    assert(world.neighbor_offsets[-1] == world.neighbors.size())
    assert(world.river_offsets[-1] == world.river_vertices.size())
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

    var repeated := generator.generate() as VoronoiWorldData
    assert(repeated != null)
    assert(repeated.sites == world.sites)
    assert(repeated.vertices == world.vertices)
    assert(repeated.elevations == world.elevations)
    assert(repeated.region_types == world.region_types)
    assert(repeated.river_vertices == world.river_vertices)
    assert(repeated.river_strengths == world.river_strengths)
    assert(repeated.river_offsets == world.river_offsets)
    assert(repeated.river_downstream_indices == world.river_downstream_indices)
    assert(repeated.cell_edge_rivers == world.cell_edge_rivers)

    var node := VoronoiWorld2D.new()
    node.settings = settings
    var node_world := node.generate() as VoronoiWorldData
    assert(node_world != null)
    assert(node_world.sites == world.sites)
    node.free()

    print("Worldgen GDExtension smoke test passed with ", world.cell_count, " cells.")
    quit(0)
