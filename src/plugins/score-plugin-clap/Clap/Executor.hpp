#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>

#include <Clap/EffectModel.hpp>
#include <clap/all.h>

#include <QByteArray>

#include <verdigris>

class QTimer;

namespace Clap
{

class Executor final
    : public Execution::ProcessComponent_T<Clap::Model, ossia::node_process>
{
  COMPONENT_METADATA("4607e18f-6400-4f93-9ce0-c79477b2124b")

public:
  static constexpr bool is_unique = true;

  Executor(Clap::Model& proc, const Execution::Context& ctx, QObject* parent);
  ~Executor();

  void cleanup() override;

private:
  //! The plug-in is no more held by this executor's node
  void releaseHandle();
  std::shared_ptr<PluginHandle> m_handle;

  // Monophonic polyphonic-pool growth (see clap_node_mono in Executor.cpp).
  // Audio thread sets m_requested_pool on the node; this timer (main thread)
  // notices, creates plug-in instances in small batches, clones state, and
  // pushes them through the SPSC queue. Monotonic — we never shrink.
  void grow_pool_tick();
  //! [main-thread] A preset loaded in the first instance of a mono node: the
  //! others get its state, clap/ext/state.h load being [main-thread] as well
  void loadPresetInPool();
  //! The instances of a mono node after the first, which this executor made
  std::vector<std::weak_ptr<PluginInstance>> m_pool;
  QTimer* m_grow_timer{};
  std::size_t m_pool_pushed{0};
  std::size_t m_pool_max_requested{0};
  double m_pool_sample_rate{0.0};
  int m_pool_buffer_size{0};
};
using ExecutorFactory = Execution::ProcessComponentFactory_T<Executor>;
}
