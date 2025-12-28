#pragma once
#include <qobjectdefs.h>

#if defined(SCORE_HAS_GPU_JS)
#include <JS/JSProcessModel.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxExecNode.hpp>

namespace JS
{
class gpu_exec_node final : public Gfx::gfx_exec_node
{
public:
  explicit gpu_exec_node(JS::ProcessModel* context, Gfx::GfxExecutionAction& ctx);

  ~gpu_exec_node();

  std::string label() const noexcept override;

  void setScript(const QString& str, JS::JSState&& new_state);

private:
  QPointer<JS::ProcessModel> m_context{};
};
}
#endif
