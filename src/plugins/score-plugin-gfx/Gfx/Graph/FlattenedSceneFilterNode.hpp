#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief Per-pass filter on a flattened scene: geometry_spec → geometry_spec.
 *
 * Reads the `filter_tag` and `filter_material_index` metadata fields that
 * ScenePreprocessorNode writes onto every output geometry, and emits a new
 * geometry_spec containing only the draws that match the configured
 * predicate. All underlying GPU buffers are shared via `shared_ptr` — the
 * filter only rewrites the mesh_list; no GPU data is copied.
 *
 * Inputs:
 *   - Port 0: Geometry (Types::Geometry)
 *   - Port 1: Filter mode (Types::Int):
 *       0  = tag equals match value
 *       1  = tag differs from match value
 *       2  = material index equals match value
 *       3  = material index differs from match value
 *       4  = blend_mode equals match (0 = opaque, 1 = premul-alpha)
 *       5  = blend_mode differs from match
 *       6  = depth_write equals (match != 0)
 *       7  = depth_write differs from (match != 0)
 *       8  = cull_mode equals match (0 = none, 1 = front, 2 = back)
 *       9  = cull_mode differs from match
 *       10 = topology equals match (0 = triangles, 1 = tri strip, …)
 *       11 = topology differs from match
 *       12 = format_id equals match_str (rapidhash of match_str truncated
 *            to 32 bits compared with filter_tag; an empty match_str
 *            short-circuits to 0u so it matches the "untagged" sentinel
 *            rather than the rapidhash of the empty string)
 *       13 = format_id differs from match_str
 *   - Port 2: Match value (Types::Int) — user-supplied, interpreted per mode
 *   - Port 3: Match string (Types::Empty control) — used by modes 12/13
 *
 * Per-draw filtering (e.g. "alphaMode=BLEND draws inside a single MDI
 * batch") is NOT handled here — ScenePreprocessor emits one geometry
 * per MDI batch so mesh-level fields collapse to 0. Use a Tier-3
 * CSF compute filter for per-draw cases; this node is for multi-mesh
 * inputs (per-object producers, pre-MDI composition).
 *
 * Outputs:
 *   - Port 0: Geometry (Types::Geometry)
 */
class SCORE_PLUGIN_GFX_EXPORT FlattenedSceneFilterNode : public ProcessNode
{
public:
  FlattenedSceneFilterNode();
  ~FlattenedSceneFilterNode() override;

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(int32_t port, const ossia::value& v) override;

  int m_mode{0};
  int m_match{0};
  std::string m_match_str;
};

}
