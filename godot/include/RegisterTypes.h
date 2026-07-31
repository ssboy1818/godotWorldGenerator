#pragma once

#include <godot_cpp/core/class_db.hpp>

namespace worldgen {

void initialize_worldgen_module(godot::ModuleInitializationLevel level);
void uninitialize_worldgen_module(godot::ModuleInitializationLevel level);

} // namespace worldgen
