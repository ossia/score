#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <QQmlEngine>
#include <QJSValue>
#include <QString>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <QEventLoop>
#include <memory>
#include <JS/Qml/QmlObjects.hpp>
#include <ossia/dataflow/node_process.hpp>

namespace Explorer
{
class DeviceDocumentPlugin;
}
namespace Device
{
class DeviceList;
}
namespace Engine
{
namespace Execution
{
class IntervalComponent;
}
}
namespace JS
{
class ProcessModel;
namespace Executor
{
class js_node final : public ossia::graph_node
{
public:
  js_node(const QString& val)
  {
    setScript(val);
  }

  void setScript(const QString& val);

  void run(ossia::token_request t, ossia::execution_state&) override;

  QQmlEngine m_engine;
  QList<std::pair<ValueInlet*, ossia::inlet_ptr>> m_valInlets;
  QList<std::pair<ValueOutlet*, ossia::outlet_ptr>> m_valOutlets;
  QList<std::pair<AudioInlet*, ossia::inlet_ptr>> m_audInlets;
  QList<std::pair<AudioOutlet*, ossia::outlet_ptr>> m_audOutlets;
  QList<std::pair<MidiInlet*, ossia::inlet_ptr>> m_midInlets;
  QList<std::pair<MidiOutlet*, ossia::outlet_ptr>> m_midOutlets;
  QObject* m_object{};
};

class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<JS::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("c2737929-231e-4d57-9088-a2a3a8d3c24e")
public:
  Component(
      JS::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~Component() override;
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}
