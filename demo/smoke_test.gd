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
    assert(settings.river_count == 12)
    assert(is_equal_approx(settings.river_minimum_source_elevation, 0.6))
    assert(is_equal_approx(settings.river_randomness, 0.25))
    settings.bounds = Rect2(0.0, 0.0, 512.0, 512.0)
    settings.seed = 93
    settings.columns = 16
    settings.rows = 16
    settings.jitter = 0.8
    settings.sea_level = 0.3
    settings.edge_decay_ratio = Vector2(0.1, 0.1)
    settings.edge_strength = 0.2
    settings.river_count = 6
    settings.river_minimum_source_elevation = 0.6
    settings.river_randomness = 0.35

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
    assert(world.river_count <= settings.river_count)
    assert(world.river_offsets.size() == world.river_count + 1)
    assert(world.river_vertices.size() == world.river_strengths.size())
    assert(world.cell_vertex_offsets[-1] == world.vertices.size())
    assert(world.neighbor_offsets[-1] == world.neighbors.size())
    assert(world.river_offsets[-1] == world.river_vertices.size())
    for river in world.river_count:
        var first_node: int = world.river_offsets[river]
        var after_last_node: int = world.river_offsets[river + 1]
        assert(after_last_node - first_node >= 2)
        for node in range(first_node, after_last_node):
            assert(world.river_strengths[node] == node - first_node + 1)

    var repeated := generator.generate() as VoronoiWorldData
    assert(repeated != null)
    assert(repeated.sites == world.sites)
    assert(repeated.vertices == world.vertices)
    assert(repeated.elevations == world.elevations)
    assert(repeated.region_types == world.region_types)
    assert(repeated.river_vertices == world.river_vertices)
    assert(repeated.river_strengths == world.river_strengths)
    assert(repeated.river_offsets == world.river_offsets)

    var node := VoronoiWorld2D.new()
    node.settings = settings
    var node_world := node.generate() as VoronoiWorldData
    assert(node_world != null)
    assert(node_world.sites == world.sites)
    node.free()

    print("Worldgen GDExtension smoke test passed with ", world.cell_count, " cells.")
    quit(0)
