#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>

#include <Clap/EffectModel.hpp>
#include <clap/all.h>

#include <verdigris>

namespace Clap
{

class Executor final
    : public Execution::ProcessComponent_T<Clap::Model, ossia::node_process>
{
  COMPONENT_METADATA("4607e18f-6400-4f93-9ce0-c79477b2124b")

public:
  static constexpr bool is_unique = true;

  Executor(Clap::Model& proc, const Execution::Context& ctx, QObject* parent);

private:
  template <typename Node_T>
  void setupNode(Node_T& node);
};
using ExecutorFactory = Execution::ProcessComponentFactory_T<Executor>;
}
