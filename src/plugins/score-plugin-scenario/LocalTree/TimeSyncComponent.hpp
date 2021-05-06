#pragma once
#include <LocalTree/LocalTreeComponent.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

namespace LocalTree
{
class SCORE_PLUGIN_SCENARIO_EXPORT TimeSync final : public CommonComponent
{
  COMMON_COMPONENT_METADATA("104e4446-b09f-4bf6-92ef-0fe360397066")
public:
  TimeSync(
      ossia::net::node_base& parent,
      Scenario::TimeSyncModel& event,
      const score::DocumentContext& doc,
      QObject* parent_comp);
  ~TimeSync();
};
}
