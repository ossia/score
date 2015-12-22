#pragma once
#include <Network/Node.h>
class ModelMetadata;
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class StateModel;
namespace Scenario
{
class ScenarioModel;

}
namespace OSSIA
{
namespace LocalTree
{

struct ScenarioVisitor
{
        void visit(Scenario::ScenarioModel& scenario,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(ModelMetadata& metadata,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(ConstraintModel& constraint,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(EventModel& ev,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(TimeNodeModel& tn,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(StateModel& state,
                   const std::shared_ptr<OSSIA::Node>& parent);
};
}
}
