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
        ISCORE_CONCRETE_FACTORY_DECL("35ba7a91-c9b1-4ba5-833c-316c0416a828")
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
