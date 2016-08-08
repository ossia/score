#pragma once
#include <Scenario/Document/State/StateModel.hpp>
#include <OSSIA/LocalTree/LocalTreeComponent.hpp>

namespace Engine
{
namespace LocalTree
{
class State final :
        public CommonComponent
{
        COMPONENT_METADATA("2e5fefa2-3442-4c08-9f3e-564ab65f7b22")
    public:
        State(
                ossia::net::node_base& parent,
                const Id<iscore::Component>& id,
                Scenario::StateModel& event,
                DocumentPlugin& doc,
                QObject* parent_comp);
};
}
}
