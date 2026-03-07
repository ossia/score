#include <Gfx/GfxForwardNode.hpp>
#include <Gfx/Graph/TextureForwardNode.hpp>

namespace Gfx
{

gfx_forward_node::gfx_forward_node(GfxExecutionAction& ctx)
    : gfx_exec_node{ctx}
{
  add_texture();     // texture input
  add_texture_out(); // texture output

  auto n = std::make_unique<score::gfx::TextureForwardNode>();
  id = exec_context->ui->register_node(std::move(n));
}

gfx_forward_node::~gfx_forward_node()
{
  if(id != score::gfx::invalid_node_index)
    exec_context->ui->unregister_node(id);
}

std::string gfx_forward_node::label() const noexcept
{
  return "Gfx::forward";
}

}
