#include "RegisterTypes.h"

#include "VoronoiWorld2D.h"
#include "VoronoiWorldData.h"
#include "VoronoiWorldGenerator.h"
#include "WorldgenSettings.h"

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace worldgen {

void initialize_worldgen_module(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE)
        return;

    GDREGISTER_CLASS(WorldgenSettings);
    GDREGISTER_CLASS(VoronoiWorldData);
    GDREGISTER_CLASS(VoronoiWorldGenerator);
    GDREGISTER_CLASS(VoronoiWorld2D);
}

void uninitialize_worldgen_module(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
}

} // namespace worldgen

extern "C" {

GDExtensionBool GDE_EXPORT worldgen_library_init(
    GDExtensionInterfaceGetProcAddress getProcAddress,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject initObject{
        getProcAddress,
        library,
        initialization,
    };

    initObject.register_initializer(worldgen::initialize_worldgen_module);
    initObject.register_terminator(worldgen::uninitialize_worldgen_module);
    initObject.set_minimum_library_initialization_level(
        godot::MODULE_INITIALIZATION_LEVEL_SCENE);

    return initObject.init();
}

} // extern "C"
