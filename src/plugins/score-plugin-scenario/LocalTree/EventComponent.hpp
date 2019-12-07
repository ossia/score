#pragma once
#include <Scenario/Document/Event/EventModel.hpp>

#include <LocalTree/LocalTreeComponent.hpp>

namespace LocalTree
{
class SCORE_PLUGIN_SCENARIO_EXPORT Event final : public CommonComponent
{
  COMMON_COMPONENT_METADATA("918b757d-d304-4362-bbd3-120f84e229fe")
public:
  Event(
      ossia::net::node_base& parent,
      const Id<score::Component>& id,
      Scenario::EventModel& event,
      const score::DocumentContext& doc,
      QObject* parent_comp);
  ~Event();

private:
  bool m_setting{};
};
}
