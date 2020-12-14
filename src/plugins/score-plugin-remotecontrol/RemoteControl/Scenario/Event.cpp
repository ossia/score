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
{
  auto si = dynamic_cast<Scenario::ScenarioInterface*>(event.parent());
  connect(
      &event, &Scenario::EventModel::statusChanged,
        this, [&, si](Scenario::ExecutionStatus st) {
        auto& parent = Scenario::parentTimeSync(event, *si);
        switch (st)
        {
          case Scenario::ExecutionStatus::Pending:
            doc.receiver.registerSync(parent);
            break;
          default:
            doc.receiver.unregisterSync(parent);
            break;
        }
      });
}
}
