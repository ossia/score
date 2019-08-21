#include "Event.hpp"

#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
namespace RemoteControl
{
Event::Event(
    const Id<score::Component>& id,
    Scenario::EventModel& event,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : Component{id, "EventComponent", parent_comp}
    , m_parent{Scenario::parentTimeSync(
          event,
          *dynamic_cast<Scenario::ScenarioInterface*>(event.parent()))}
{
  connect(
      &event,
      &Scenario::EventModel::statusChanged,
      this,
      [&](Scenario::ExecutionStatus st) {
        switch (st)
        {
          case Scenario::ExecutionStatus::Pending:
            doc.receiver.registerSync(m_parent);
            break;
          default:
            doc.receiver.unregisterSync(m_parent);
            break;
        }
      });
}
}
