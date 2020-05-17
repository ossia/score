#pragma once
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>

namespace Scenario
{
class TimeSyncModel;
}
namespace score
{
class Command;
} // namespace score

class LoopTriggerCommandFactory : public Scenario::Command::TriggerCommandFactory
{
  SCORE_CONCRETE("dd32c40b-bf76-4f6c-a8e4-25132ec61d93")
public:
  bool matches(const Scenario::TimeSyncModel& tn) const override;

  score::Command* make_addTriggerCommand(const Scenario::TimeSyncModel& tn) const override;

  score::Command* make_removeTriggerCommand(const Scenario::TimeSyncModel& tn) const override;
};
