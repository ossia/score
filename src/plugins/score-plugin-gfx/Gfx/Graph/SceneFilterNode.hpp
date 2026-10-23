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
 *        2 = keep only the subtrees whose node name matches the "Name"
 *            pattern, with their parent chain (filterScene)
 *   - Port 2: Name (a string): the pattern of mode 2
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
  std::string m_name;
};

/**
 * @brief The Scene Filter's rewrite of a scene, for a mode and a name pattern.
 *
 * Mode 2 keeps the subtrees whose node name matches @p pattern: libossia's OSC
 * pattern syntax (`*`, `?`, `[0-9]`, `{a,b}`), matched against the whole name.
 * A matching node is kept whole. A node kept only because something below it
 * matches keeps its transforms and its kept sub-nodes, so the match stays where
 * it was, and drops its own components. An empty pattern keeps everything.
 * Modes 0 and 1 ignore the pattern.
 */
SCORE_PLUGIN_GFX_EXPORT
ossia::scene_spec
filterScene(const ossia::scene_spec& in, int mode, std::string_view pattern);

}
