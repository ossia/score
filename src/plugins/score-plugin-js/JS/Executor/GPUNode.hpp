#pragma once
#include <memory>
#include <qobjectdefs.h>

#if defined(SCORE_HAS_GPU_JS)
#include <JS/JSProcessModel.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/Node.hpp>
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
  explicit gpu_exec_node(Gfx::GfxExecutionAction& ctx);

  ~gpu_exec_node();

  std::string label() const noexcept override;

  //! Execution-thread half of a script change: drops the running node and
  //! builds its replacement, without touching the UI-thread process. The
  //! caller finishes it on the UI thread with attachAndRegister().
  //! The node is a JS::GpuNode (defined in GPUNode.cpp).
  std::unique_ptr<score::gfx::Node>
  prepareScript(const QString& root, const QString& str, JS::JSState&& new_state);
  //! UI-thread half: connects the new node to its live process and registers
  //! it with the gfx context. Returns the new node id, to be stored in `id`
  //! from the execution thread.
  int32_t
  attachAndRegister(std::unique_ptr<score::gfx::Node> n, JS::ProcessModel& process);
  //! Execution thread: takes the id attachAndRegister returned, unless a newer
  //! script change superseded it in the meantime (then that node is dropped).
  void adoptId(int32_t new_id, int expected_script_index);
  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override;

private:
  std::shared_ptr<GpuValueQueue> m_valueMessages;
};
}
#endif
