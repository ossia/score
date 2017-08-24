#pragma once
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>

namespace Scenario
{
class TimeSyncModel;
}
namespace iscore
{
class Command;
} // namespace iscore

class LoopTriggerCommandFactory
    : public Scenario::Command::TriggerCommandFactory
{
  ISCORE_CONCRETE("dd32c40b-bf76-4f6c-a8e4-25132ec61d93")
public:
  bool matches(const Scenario::TimeSyncModel& tn) const override;

  iscore::Command*
  make_addTriggerCommand(const Scenario::TimeSyncModel& tn) const override;

  iscore::Command*
  make_removeTriggerCommand(const Scenario::TimeSyncModel& tn) const override;
};
