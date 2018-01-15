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
class SCORE_PLUGIN_ENGINE_EXPORT EventComponent final :
    public Execution::Component
{
  Q_OBJECT
  COMMON_COMPONENT_METADATA("02c41de0-3a8c-44da-ae03-68a0ca26a7d0")
public:
  EventComponent(
      const Scenario::EventModel& element,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void cleanup();

  //! To be called from the GUI thread
  ossia::expression_ptr makeExpression() const;

  //! To be called from the API edition queue
  void onSetup(std::shared_ptr<ossia::time_event> event,
               ossia::expression_ptr expr,
               ossia::time_event::offset_behavior b);

  std::shared_ptr<ossia::time_event> OSSIAEvent() const;
  const Scenario::EventModel& scoreEvent() const
  {
    return m_score_event;
  }

Q_SIGNALS:
  void happened();

private:
  const Scenario::EventModel& m_score_event;
  std::shared_ptr<ossia::time_event> m_ossia_event;
};
}
}
