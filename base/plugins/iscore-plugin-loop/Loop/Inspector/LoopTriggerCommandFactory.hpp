#pragma once
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>

namespace Scenario
{
class TimeNodeModel;
}
namespace iscore {
class SerializableCommand;
}  // namespace iscore

class LoopTriggerCommandFactory : public Scenario::Command::TriggerCommandFactory
{
    public:
        bool matches(
                const Scenario::TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_addTriggerCommand(
                const Scenario::TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_removeTriggerCommand(
                const Scenario::TimeNodeModel& tn) const override;
};
