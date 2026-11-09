#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <Vst3/EffectModel.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <score_plugin_vst3_export.h>

#include <memory>

#include <verdigris>

namespace vst3
{
class vst_node_base;

//! Sends the changes made on the UI to `ctrl` to the parameter queue
//! `queue_idx` of `node`, through the execution queue: the node is only
//! touched from the audio thread, between two ticks, and the plug-in gets
//! the change in the next block.
SCORE_PLUGIN_VST3_EXPORT
QMetaObject::Connection connectControl(
    const Process::ControlInlet& ctrl, QObject* context,
    Execution::ExecutionCommandQueue& queue, std::weak_ptr<vst_node_base> node,
    std::size_t queue_idx);

class Executor final
    : public Execution::ProcessComponent_T<vst3::Model, ossia::node_process>
{
  W_OBJECT(Executor)
  COMPONENT_METADATA("a9f2c738-d22b-4654-9f58-25f25f099d79")

public:
  static constexpr bool is_unique = true;

  Executor(vst3::Model& proc, const Execution::Context& ctx, QObject* parent);

  void cleanup() override;

private:
  template <typename Node_T>
  void setupNode(Node_T& node);
};
using ExecutorFactory = Execution::ProcessComponentFactory_T<Executor>;
}
