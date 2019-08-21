#pragma once
#include <Scenario/Document/Event/EventModel.hpp>

#include <RemoteControl/DocumentPlugin.hpp>

namespace RemoteControl
{
class Event final : public score::Component
{
  COMMON_COMPONENT_METADATA("c12629a1-e77b-42e8-a2dc-9b6532a011c1")
public:
  Event(
      const Id<score::Component>& id,
      Scenario::EventModel& event,
      DocumentPlugin& doc,
      QObject* parent_comp);

private:
  Path<Scenario::TimeSyncModel> m_parent;
};
}
