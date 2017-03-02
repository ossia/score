#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <QQmlEngine>
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
class ConstraintComponent;
}
}
namespace JS
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final : public ossia::time_process
{
public:
  ProcessExecutor(const Explorer::DeviceDocumentPlugin& devices);

  void setTickFun(const QString& val);

  ossia::state_element state(double);
  ossia::state_element state() override;
  ossia::state_element offset(ossia::time_value) override;

private:
  const Device::DeviceList& m_devices;
  QQmlEngine m_engine;
  QObject* m_object{};
  QJSValue m_tickFun;
};

class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<JS::ProcessModel, ProcessExecutor>
{
  COMPONENT_METADATA("c2737929-231e-4d57-9088-a2a3a8d3c24e")
public:
  Component(
      Engine::Execution::ConstraintComponent& parentConstraint,
      JS::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}
