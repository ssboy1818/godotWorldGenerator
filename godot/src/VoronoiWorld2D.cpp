#include "VoronoiWorld2D.h"

#include <godot_cpp/core/class_db.hpp>

namespace worldgen {

void VoronoiWorld2D::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_settings", "settings"),
                                &VoronoiWorld2D::setSettings);
    godot::ClassDB::bind_method(godot::D_METHOD("get_settings"),
                                &VoronoiWorld2D::settings);
    godot::ClassDB::bind_method(godot::D_METHOD("generate"),
                                &VoronoiWorld2D::generate);
    godot::ClassDB::bind_method(godot::D_METHOD("generate_async"),
                                &VoronoiWorld2D::generateAsync);

    ADD_PROPERTY(godot::PropertyInfo(godot::Variant::OBJECT,
                                     "settings",
                                     godot::PROPERTY_HINT_RESOURCE_TYPE,
                                     "WorldgenSettings",
                                     godot::PROPERTY_USAGE_DEFAULT
                                         | godot::PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT),
                 "set_settings", "get_settings");
}

VoronoiWorld2D::VoronoiWorld2D() {
    m_generator.instantiate();
}

void VoronoiWorld2D::setSettings(
    const godot::Ref<WorldgenSettings> &settings) {
    m_generator->setSettings(settings);
}

godot::Ref<WorldgenSettings> VoronoiWorld2D::settings() const {
    return m_generator->settings();
}

godot::Ref<VoronoiWorldData> VoronoiWorld2D::generate() {
    return m_generator->generate();
}

godot::Ref<VoronoiWorldGenerationTask> VoronoiWorld2D::generateAsync() {
    return m_generator->generateAsync();
}

} // namespace worldgen
