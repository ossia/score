#pragma once
#include <qobjectdefs.h>

#if defined(SCORE_HAS_GPU_JS)
#include <JS/JSProcessModel.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <ossia/detail/lockfree_queue.hpp>

namespace JS
{
struct GpuValueMessage
{
  std::size_t outlet{};
  ossia::value value;
};
using GpuValueQueue = ossia::mpmc_queue<GpuValueMessage>;

class gpu_exec_node final : public Gfx::gfx_exec_node
{
public:
  explicit gpu_exec_node(JS::ProcessModel* context, Gfx::GfxExecutionAction& ctx);

  ~gpu_exec_node();

  std::string label() const noexcept override;

  void setScript(const QString& root, const QString& str, JS::JSState&& new_state);
  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override;

private:
  QPointer<JS::ProcessModel> m_context{};
  std::shared_ptr<GpuValueQueue> m_valueMessages;
};
}
#endif
