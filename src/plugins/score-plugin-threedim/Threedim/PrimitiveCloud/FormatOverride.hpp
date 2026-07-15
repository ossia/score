#pragma once

#include <ossia/dataflow/geometry_port.hpp>

#include <memory>
#include <string_view>

namespace Threedim::PrimitiveCloud
{

// Shallow-clones `state` and rewrites every primitive_cloud_component
// reachable through the scene tree to carry `override` as its
// `format_id`. Heavy fields (raw_data buffer_resource, extra_buffers,
// bounds, …) are shared via shared_ptr — no GPU upload duplicates.
//
// Used by AssetLoader's "Format override" line edit and the TagAs
// pass-through node so unrecognised PLY columns / procedural producers
// without an autodetected format_id can still be routed by a
// FlattenedSceneFilterNode in mode 12 (format_id == match_str).
//
// `override.empty()` returns the input verbatim (`const_pointer_cast`
// to drop the const, but no actual mutation is performed). A null
// `state` returns null. Otherwise the returned shared_ptr is freshly
// allocated; its `version` and `dirty_index` are bumped by 1 so
// downstream change-detection sees a fresh frame.
//
// Walks scene_node children recursively. Nested scene_node_ptr inside
// children is itself deep-cloned so the rewrite is leak-free for the
// const tree shape.
std::shared_ptr<ossia::scene_state> applyFormatOverride(
    std::shared_ptr<const ossia::scene_state> state,
    std::string_view override);

} // namespace Threedim::PrimitiveCloud
