#pragma once
#include <JS/Qml/QmlObjects.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/scenario/time_value.hpp>


#include <memory>

namespace JS
{
class ProcessModel;
namespace Executor
{
class Component final
    : public ::Execution::
          ProcessComponent_T<JS::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("c2737929-231e-4d57-9088-a2a3a8d3c24e")
public:
  Component(
      JS::ProcessModel& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
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
