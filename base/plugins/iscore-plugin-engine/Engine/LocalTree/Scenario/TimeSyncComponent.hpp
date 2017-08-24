#pragma once
#include <Engine/LocalTree/LocalTreeComponent.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

namespace Engine
{
namespace LocalTree
{
class TimeSync final : public CommonComponent
{
  COMMON_COMPONENT_METADATA("104e4446-b09f-4bf6-92ef-0fe360397066")
public:
  TimeSync(
      ossia::net::node_base& parent,
      const Id<iscore::Component>& id,
      Scenario::TimeSyncModel& event,
      DocumentPlugin& doc,
      QObject* parent_comp);
};
}
}
