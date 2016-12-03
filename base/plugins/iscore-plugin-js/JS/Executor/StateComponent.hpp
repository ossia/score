#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/StateProcessComponent.hpp>
#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <memory>

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
class ConstraintElement;
}
}
namespace ossia
{
class state;
} // namespace OSSIA

namespace JS
{
class StateProcess;
namespace Executor
{
class State
{
public:
  State(const QString& script, const Explorer::DeviceDocumentPlugin& devices);

  void operator()();

  const Device::DeviceList& m_devices;
  std::shared_ptr<QJSEngine> m_engine;
  QJSValue m_fun;
};

class StateProcessComponent final
    : public Engine::Execution::StateProcessComponent_T<JS::StateProcess>
{
  COMPONENT_METADATA("068c116f-9d1f-47d0-bd43-335792ba1a6a")
public:
  StateProcessComponent(
      Engine::Execution::StateElement& parentState,
      JS::StateProcess& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  static ossia::state_element
  make(Process::StateProcess& proc, const Engine::Execution::Context& ctxt);
};

using StateProcessComponentFactory
    = Engine::Execution::StateProcessComponentFactory_T<StateProcessComponent>;
}
}
