#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief A node that forwards an input texture to its output unchanged.
 *
 * Used for texture propagation through the interval/scenario hierarchy,
 * mirroring how audio uses forward_node.
 */
class SCORE_PLUGIN_GFX_EXPORT TextureForwardNode : public NodeModel
{
public:
  TextureForwardNode();
  ~TextureForwardNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;
  void process(Message&& msg) override;
};

}
