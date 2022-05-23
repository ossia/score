#pragma once
#include <ControlSurface/Process.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionSetup.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/safe_nodes/executor.hpp>
#include <ossia/detail/ssize.hpp>

namespace ossia
{
class control_surface_node;
}
namespace ControlSurface
{
class Model;

class ProcessExecutorComponent final
    : public Execution::
          ProcessComponent_T<ControlSurface::Model, ossia::node_process>
{
  COMPONENT_METADATA("bab572b1-37eb-4f32-8f72-d5b79b65cfe9")
public:
  ProcessExecutorComponent(
      ControlSurface::Model& element,
      const ::Execution::Context& ctx,
      QObject* parent);

  void inletAdded(Process::ControlInlet& inl);

  ~ProcessExecutorComponent();

private:
  int m_currentIndex{};
};
using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
