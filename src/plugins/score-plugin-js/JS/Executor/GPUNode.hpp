#pragma once
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxExecNode.hpp>

namespace JS
{
class GpuNode;

class gpu_exec_node final : public Gfx::gfx_exec_node
{
public:
  explicit gpu_exec_node(Gfx::GfxExecutionAction& ctx);

  ~gpu_exec_node();

  std::string label() const noexcept override;

  void setScript(const QString& str);
};
}
