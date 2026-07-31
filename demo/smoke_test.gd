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
    settings.bounds = Rect2(-16.0, -8.0, 64.0, 48.0)
    settings.seed = 42
    settings.columns = 4
    settings.rows = 3
    settings.jitter = 0.7

    var generator := VoronoiWorldGenerator.new()
    generator.settings = settings
    var world := generator.generate() as VoronoiWorldData
    assert(world != null)
    assert(world.cell_count == 12)
    assert(world.sites.size() == world.cell_count)
    assert(world.cell_vertex_offsets.size() == world.cell_count + 1)
    assert(world.neighbor_offsets.size() == world.cell_count + 1)
    assert(world.elevations.size() == world.cell_count)
    assert(world.region_types.size() == world.cell_count)
    assert(world.cell_vertex_offsets[-1] == world.vertices.size())
    assert(world.neighbor_offsets[-1] == world.neighbors.size())

    var repeated := generator.generate() as VoronoiWorldData
    assert(repeated != null)
    assert(repeated.sites == world.sites)
    assert(repeated.vertices == world.vertices)
    assert(repeated.elevations == world.elevations)
    assert(repeated.region_types == world.region_types)

    var node := VoronoiWorld2D.new()
    node.settings = settings
    var node_world := node.generate() as VoronoiWorldData
    assert(node_world != null)
    assert(node_world.sites == world.sites)
    node.free()

    print("Worldgen GDExtension smoke test passed with ", world.cell_count, " cells.")
    quit(0)
