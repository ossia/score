#pragma once
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>

class TimeNodeModel;
namespace iscore {
class SerializableCommand;
}  // namespace iscore

class ScenarioTriggerCommandFactory : public TriggerCommandFactory
{
    public:
        bool matches(
                const TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_addTriggerCommand(
                const TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_removeTriggerCommand(
                const TimeNodeModel& tn) const override;
};
