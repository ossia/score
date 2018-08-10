#pragma once
#include <ossia/editor/expression/expression.hpp>
#include <wobjectdefs.h>
#include <ossia/editor/scenario/time_event.hpp>

#include <Process/ExecutionComponent.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
namespace ossia
{
class time_event;
}

namespace Execution
{
class SCORE_PLUGIN_SCENARIO_EXPORT EventComponent final
    : public Execution::Component
{
  W_OBJECT(EventComponent)
  COMMON_COMPONENT_METADATA("02c41de0-3a8c-44da-ae03-68a0ca26a7d0")
public:
  EventComponent(
      const Scenario::EventModel& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void cleanup();

  //! To be called from the GUI thread
  ossia::expression_ptr makeExpression() const;

  //! To be called from the API edition queue
  void onSetup(
      std::shared_ptr<ossia::time_event> event,
      ossia::expression_ptr expr,
      ossia::time_event::offset_behavior b);

  std::shared_ptr<ossia::time_event> OSSIAEvent() const;
  const Scenario::EventModel& scoreEvent() const
  {
    SCORE_ASSERT(m_score_event);
    return *m_score_event;
  }

public:
  void happened() W_SIGNAL(happened);

private:
  QPointer<const Scenario::EventModel> m_score_event;
  std::shared_ptr<ossia::time_event> m_ossia_event;
};
}

