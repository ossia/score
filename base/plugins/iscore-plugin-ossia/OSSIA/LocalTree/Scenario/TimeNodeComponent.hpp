#pragma once
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <OSSIA/LocalTree/LocalTreeComponent.hpp>

namespace Ossia
{
namespace LocalTree
{
class TimeNode final :
        public CommonComponent
{
        COMPONENT_METADATA("104e4446-b09f-4bf6-92ef-0fe360397066")
    public:
        TimeNode(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                Scenario::TimeNodeModel& event,
                DocumentPlugin& doc,
                QObject* parent_comp);
};
}
}
