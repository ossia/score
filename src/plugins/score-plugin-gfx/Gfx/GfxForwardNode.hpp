#pragma once
#include <Gfx/GfxExecNode.hpp>

namespace Gfx
{

/**
 * @brief Execution node for texture forwarding/propagation.
 *
 * Created per-interval and per-scenario to forward texture outputs
 * from child processes up the hierarchy, mirroring how audio propagation
 * uses ossia::nodes::forward_node.
 *
 * Has 1 texture inlet and 1 texture outlet. The base class gfx_exec_node::run()
 * handles link_cable_to_inlet() and push_texture() automatically.
 */
class SCORE_PLUGIN_GFX_EXPORT gfx_forward_node final : public gfx_exec_node
{
public:
  gfx_forward_node(GfxExecutionAction& ctx);
  ~gfx_forward_node();
  std::string label() const noexcept override;
};

}
