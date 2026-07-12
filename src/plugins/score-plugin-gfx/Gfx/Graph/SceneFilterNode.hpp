#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief Tree-level filter on a scene_spec.
 *
 * Walks the incoming scene hierarchy and rebuilds it with only the
 * subtrees matching the predicate. Runs on the render thread but does
 * exclusively CPU work — no GPU allocation; shared_ptr reuse keeps cost
 * minimal when the scene is unchanged.
 *
 * Inputs:
 *   - Port 0: Scene (Types::Scene)
 *   - Port 1: Mode (Types::Int):
 *        0 = pass-through (no filtering)
 *        1 = keep only scene_nodes with visible == true
 *        2 = keep only subtrees whose node name contains the substring set
 *            in the "Name" control (future-wired; string port missing in the
 *            renderer for now, so behaves like mode 1 until wired)
 *
 * Outputs:
 *   - Port 0: Scene (Types::Scene)
 */
class SCORE_PLUGIN_GFX_EXPORT SceneFilterNode : public ProcessNode
{
public:
  SceneFilterNode();
  ~SceneFilterNode() override;

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(int32_t port, const ossia::value& v) override;

  int m_mode{0};
};

}
