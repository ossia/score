#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief Whether the Scene Preprocessor can copy a GPU index buffer of this
 *        index size into its uint32 index stream on this backend.
 *
 * uint16 indices are copied two bytes at a time into the low half of each
 * slot, which Metal on macOS refuses: it only blits 4-byte-aligned ranges.
 */
SCORE_PLUGIN_GFX_EXPORT bool
copiesGpuIndices(QRhi::Implementation backend, ossia::index_format format) noexcept;

/**
 * @brief Whether GPU indices of this size are widened to uint32 by a compute
 *        pass instead of copied: uint16 ones where copiesGpuIndices refuses.
 */
SCORE_PLUGIN_GFX_EXPORT bool
widensGpuIndices(QRhi::Implementation backend, ossia::index_format format) noexcept;

/**
 * @brief Widen uint16 GPU indices with the compute pass on every backend, so
 *        that the Metal path can be tested elsewhere.
 */
SCORE_PLUGIN_GFX_EXPORT void forceGpuIndexWidening(bool force) noexcept;

/**
 * @brief Bridge from `scene_spec` (hierarchical, CPU) to `geometry_spec`
 *        (flat, GPU-resident).
 *
 * Receives a `scene_spec` on its input port, walks the hierarchy, and emits
 * a `geometry_spec` on its output port containing one geometry per scene
 * mesh primitive. Each output geometry carries a set of well-known
 * auxiliary buffers:
 *
 *   - `scene_lights`           : LightGPU[]     (per scene light_component)
 *   - `scene_materials`        : MaterialGPU[]  (per scene material)
 *   - `scene_materials_ext`    : MaterialExtGPU[] (extended material data)
 *   - `per_draws`              : PerDrawGPU[]   (one per draw: model/normal mat,
 *                                                material/transform/skeleton slots)
 *   - `indirect_draw_cmds`     : IndirectCmd[]  (MDI command buffer; one per draw)
 *   - `scene_counts`           : SceneCountsUBO (draw/light/material counts)
 *   - `camera`                 : CameraUBO      (current-frame camera matrices)
 *   - `camera_prev`            : CameraUBO      (previous-frame camera matrices)
 *   - `env`                    : EnvUBO         (environment/fog parameters)
 *   - `world_transforms`       : mat4[]         (current frame, slot-indexed)
 *   - `world_transforms_prev`  : mat4[]         (previous frame, for TAA/motion)
 *   - `scene_light_indices`    : uint[]         (light culling index list)
 *
 * Conditionally emitted (when present in the scene):
 *   - `scene_material_uv_xforms` : mat3[]       (per-material UV transforms)
 *   - `per_draw_bounds`          : AABB[]        (per-draw world-space bounds)
 *   - `shadow_cascades`          : CascadeUBO[]  (shadow cascade matrices)
 *
 * Per-draw indexing in shaders uses the MDI `firstInstance` / `gl_DrawID`
 * mechanism. Shaders read `per_draws[gl_DrawID]` to recover model/normal
 * matrices and slot indices into the shared tables.
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
