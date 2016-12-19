#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <QString>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>
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

namespace RecordedMessages
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final : public ossia::time_process
{
public:
  ProcessExecutor(
      const Explorer::DeviceDocumentPlugin& devices,
      const RecordedMessagesList& lst);

  ossia::state_element state(double);
  ossia::state_element state() override;
  ossia::state_element offset(ossia::time_value) override;

private:
  const Device::DeviceList& m_devices;
  RecordedMessagesList m_list;
};

class Component final
    : public ::Engine::Execution::
          ProcessComponent_T<RecordedMessages::ProcessModel, ProcessExecutor>
{
  COMPONENT_METADATA("bfcdcd2a-be3c-4bb1-bcca-240a6435b06b")
public:
  Component(
      Engine::Execution::ConstraintElement& parentConstraint,
      RecordedMessages::ProcessModel& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);
};

using ComponentFactory
    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}
