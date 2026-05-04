#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief Bridge from `scene_spec` (hierarchical, CPU) to `geometry_spec`
 *        (flat, GPU-resident).
 *
 * Receives a `scene_spec` on its input port, walks the hierarchy, and emits
 * a `geometry_spec` on its output port containing one geometry per scene
 * mesh primitive. Each output geometry carries a set of well-known
 * auxiliary buffers shared across all draws:
 *
 *   - `scene_lights`    : LightGPU[]  (per scene_payload light_component)
 *   - `scene_materials` : MaterialGPU[]  (per scene material)
 *   - `model_matrices`  : mat4[]  (one per draw, in scene-walk order)
 *   - `draw_cmds`       : DrawCmdMeta[]  (per draw: material_index + padding)
 *
 * Plus a per-mesh aux `this_draw` carrying the draw index into the shared
 * tables, so consumer shaders can look up `model_matrices[this_draw.idx]`
 * etc. without needing `gl_DrawID` / multi-draw indirect.
 *
 * The auxiliary layouts are also documented in the shipped
 * `scene_preprocessor.csf` packer shaders — they are the canonical
 * source of truth. C++ here just packs identical bytes.
 *
 * Inputs:
 *   - Port 0: Scene (Types::Scene)
 *
 * Outputs:
 *   - Port 0: Geometry (Types::Geometry) — flattened scene
 */
class SCORE_PLUGIN_GFX_EXPORT ScenePreprocessorNode : public ProcessNode
{
public:
  ScenePreprocessorNode();
  ~ScenePreprocessorNode() override;

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;
};

}
