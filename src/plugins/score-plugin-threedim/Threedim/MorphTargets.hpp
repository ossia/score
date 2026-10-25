#pragma once
#include <ossia/dataflow/geometry_port.hpp>

#include <span>

namespace Threedim
{

// Morph targets (glTF) / shape keys (Blender), blended on the CPU: the
// renderer only knows vertex buffers.
//
// Layout convention for ossia::mesh_component::morph_targets: target t's
// position_deltas (and normal_deltas, when present) hold float3 deltas for
// every vertex of every primitive, primitive after primitive in the order of
// mesh_component::primitives, each over its vertex_count. A null buffer means
// that target does not move that attribute.
//
// mesh_component::morph_weights are the weights the primitives' POSITION and
// NORMAL buffers are currently blended with.

// The mesh with its POSITION and NORMAL buffers blended to `weights` instead
// of mesh.morph_weights, its bounds recomputed and morph_weights set to
// `weights`. Missing weights count as 0. Buffers that no target touches are
// shared with `mesh`; everything else in it is copied as is.
//
// Blending is relative to the mesh's current weights, so passing the same
// source mesh with successive weights never accumulates rounding error.
std::shared_ptr<ossia::mesh_component>
blendMorphTargets(const ossia::mesh_component& mesh, std::span<const float> weights);

}
