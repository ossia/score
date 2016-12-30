#pragma once
#include <Engine/Executor/Component.hpp>

namespace ossia
{
class time_event;
}
namespace Scenario
{
class EventModel;
}

namespace Engine
{
namespace Execution
{
class ISCORE_PLUGIN_ENGINE_EXPORT EventComponent final :
    public Execution::Component
{
  Q_OBJECT
  COMMON_COMPONENT_METADATA("02c41de0-3a8c-44da-ae03-68a0ca26a7d0")
public:
  EventComponent(
      std::shared_ptr<ossia::time_event> event,
      const Scenario::EventModel& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  std::shared_ptr<ossia::time_event> OSSIAEvent() const;
  const Scenario::EventModel& iscoreEvent() const
  {
    return m_iscore_event;
  }

signals:
  void happened();

private:
  const Scenario::EventModel& m_iscore_event;
  std::shared_ptr<ossia::time_event> m_ossia_event;
};
}
}
