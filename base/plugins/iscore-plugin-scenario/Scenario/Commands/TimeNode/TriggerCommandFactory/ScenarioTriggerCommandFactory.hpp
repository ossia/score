#pragma once
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>

namespace iscore {
class SerializableCommand;
}  // namespace iscore
namespace Scenario
{
class TimeNodeModel;
namespace Command
{

class ScenarioTriggerCommandFactory : public TriggerCommandFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("26e38b07-13fa-4f6d-9b95-1bdaeeafab9e")
    public:
        bool matches(
                const TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_addTriggerCommand(
                const TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_removeTriggerCommand(
                const TimeNodeModel& tn) const override;
};
}
}
