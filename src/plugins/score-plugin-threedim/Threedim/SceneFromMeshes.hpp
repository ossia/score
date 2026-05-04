#pragma once
#include <Threedim/TinyObj.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <memory>
#include <string_view>

namespace Threedim
{

// Convert a vector of Threedim::mesh (produced by TinyObjFromFile,
// PlyFromFile, or the new vcglib-STL / vcglib-OFF bridges) into a
// scene_state containing one scene_node per mesh part, each with a
// mesh_component backing onto a single shared CPU buffer.
//
// All mesh parts share the same `float_vec` — the scene's mesh_primitives
// reference it via buffer_resource_ptr with per-attribute byte offsets
// into the same vertex buffer. This matches the layout tinyobj / miniply
// already produce: attributes are non-interleaved, each one a contiguous
// span in the parent buffer, with pos_offset / texcoord_offset / …
// in *elements* (floats), not bytes.
//
// `source_label` is used as the scene_node name; it should be the source
// filename (or `.` when unknown), purely for inspector readability.
//
// On empty input returns a null pointer; caller keeps the previous state.
std::shared_ptr<ossia::scene_state> sceneStateFromMeshes(
    std::vector<Threedim::mesh> meshes,
    Threedim::float_vec buffer,
    std::string_view source_label = {});

}
