#pragma once
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>

namespace iscore
{
class Command;
} // namespace iscore

namespace Scenario
{
class TimeSyncModel;
namespace Command
{
class BaseScenarioTriggerCommandFactory : public TriggerCommandFactory
{
  ISCORE_CONCRETE("35ba7a91-c9b1-4ba5-833c-316c0416a828")
public:
  bool matches(const TimeSyncModel& tn) const override;

  iscore::Command*
  make_addTriggerCommand(const TimeSyncModel& tn) const override;

  iscore::Command*
  make_removeTriggerCommand(const TimeSyncModel& tn) const override;
};
}
}
