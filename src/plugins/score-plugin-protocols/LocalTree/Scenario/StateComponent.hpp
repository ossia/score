#pragma once
#include <Scenario/Document/State/StateModel.hpp>

#include <LocalTree/LocalTreeComponent.hpp>

namespace LocalTree
{
class State final : public CommonComponent
{
  COMMON_COMPONENT_METADATA("2e5fefa2-3442-4c08-9f3e-564ab65f7b22")
public:
  State(
      ossia::net::node_base& parent,
      const Id<score::Component>& id,
      Scenario::StateModel& event,
      DocumentPlugin& doc,
      QObject* parent_comp);
};
}
