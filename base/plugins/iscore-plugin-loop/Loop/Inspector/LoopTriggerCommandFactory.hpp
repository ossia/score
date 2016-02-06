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
        ISCORE_CONCRETE_FACTORY_DECL("dd32c40b-bf76-4f6c-a8e4-25132ec61d93")
    public:
        bool matches(
                const Scenario::TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_addTriggerCommand(
                const Scenario::TimeNodeModel& tn) const override;

        iscore::SerializableCommand* make_removeTriggerCommand(
                const Scenario::TimeNodeModel& tn) const override;
};
