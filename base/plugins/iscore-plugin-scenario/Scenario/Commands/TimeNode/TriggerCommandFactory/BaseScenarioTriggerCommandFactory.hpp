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
class BaseScenarioTriggerCommandFactory : public TriggerCommandFactory
{
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
