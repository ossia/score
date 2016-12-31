#pragma once
#include <Engine/Executor/Component.hpp>
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/time_event.hpp>

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
      const Scenario::EventModel& element,
      const Engine::Execution::Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent);

  void cleanup();

  //! To be called from the GUI thread
  ossia::expression_ptr makeExpression() const;

  //! To be called from the API edition queue
  void onSetup(std::shared_ptr<ossia::time_event> event,
               ossia::expression_ptr expr,
               ossia::time_event::OffsetBehavior b);

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
