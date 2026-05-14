#pragma once

#include <ossia/dataflow/geometry_port.hpp>

#include <memory>
#include <string_view>

namespace Threedim::PrimitiveCloud
{

// Wrap a parsed primitive_cloud_component into a fresh scene_state with
// one scene_node carrying it as its sole payload. Mirrors
// SceneFromMeshes::sceneStateFromMeshes for the splat path.
//
// `source_label` becomes the scene_node name (typically the source
// filename). Returns nullptr if `cloud` is null.
std::shared_ptr<ossia::scene_state> sceneStateFromCloud(
    ossia::primitive_cloud_component_ptr cloud,
    std::string_view source_label = {});

} // namespace Threedim::PrimitiveCloud
