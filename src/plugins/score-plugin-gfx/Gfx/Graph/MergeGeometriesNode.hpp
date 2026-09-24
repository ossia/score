#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief Concatenates up to N upstream geometry_specs into one.
 *
 * Intended use: combine independently-flattened scene partitions (static
 * environment + animated characters + CSF-produced particles) into a
 * single geometry_spec that a single downstream renderer can draw in one
 * pass. Inputs without a transform share their buffers; only the top-level
 * mesh_list is rebuilt. An input with a transform gets baked copies: on the
 * CPU for CPU-resident attributes, and through a compute pass into buffers
 * owned by the renderer for GPU-resident ones.
 *
 * For v1, up to 8 input geometry ports are exposed. Unconnected ports
 * contribute nothing.
 *
 * Inputs:
 *   - Port 0..7: Geometry (Types::Geometry)
 *
 * Outputs:
 *   - Port 0: Geometry (Types::Geometry)
 */
/**
 * @brief Copies of the meshes with a model transform applied to their CPU
 * position, normal, tangent and bitangent attributes and to their bounds.
 *
 * GPU-resident buffers are left as they are.
 */
SCORE_PLUGIN_GFX_EXPORT
std::vector<ossia::geometry> bakeGeometryTransform(
    const std::vector<ossia::geometry>& meshes, const ossia::transform3d& transform);

class SCORE_PLUGIN_GFX_EXPORT MergeGeometriesNode : public ProcessNode
{
public:
  static constexpr int kMaxInputs = 8;

  MergeGeometriesNode();
  ~MergeGeometriesNode() override;

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;
};

}
