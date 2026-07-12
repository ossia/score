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
 * pass. All underlying GPU buffers are shared via `shared_ptr`; only the
 * top-level mesh_list is rebuilt.
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
class SCORE_PLUGIN_GFX_EXPORT MergeGeometriesNode : public ProcessNode
{
public:
  static constexpr int kMaxInputs = 8;

  MergeGeometriesNode();
  ~MergeGeometriesNode() override;

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;
};

}
