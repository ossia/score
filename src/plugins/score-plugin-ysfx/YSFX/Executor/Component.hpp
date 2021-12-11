#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <memory>

namespace YSFX
{
class ProcessModel;
namespace Executor
{
class Component final
    : public ::Execution::
          ProcessComponent_T<YSFX::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("bf31e029-5695-4cd0-8c68-dbb423db0db7")
public:
  Component(
      YSFX::ProcessModel& element,
      const Execution::Context& ctx,
      QObject* parent);
  ~Component() override;

private:
  void on_scriptChange(const QString& script);
  Process::Inlets m_oldInlets;
  Process::Outlets m_oldOutlets;
};

using ComponentFactory = ::Execution::ProcessComponentFactory_T<Component>;
}
}
