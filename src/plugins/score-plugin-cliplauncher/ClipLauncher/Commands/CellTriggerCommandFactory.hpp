#pragma once
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>

namespace ClipLauncher
{

class CellTriggerCommandFactory final
    : public Scenario::Command::TriggerCommandFactory
{
  SCORE_CONCRETE("d4e5f6a7-8b9c-0d1e-2f3a-4b5c6d7e8f90")
public:
  bool matches(const Scenario::TimeSyncModel& tn) const override;
  score::Command* make_addTriggerCommand(const Scenario::TimeSyncModel& tn) const override;
  score::Command* make_removeTriggerCommand(const Scenario::TimeSyncModel& tn) const override;
};

} // namespace ClipLauncher

// Trigger command declarations for the command list generator
#include <Scenario/Commands/TimeSync/AddTrigger.hpp>
#include <Scenario/Commands/TimeSync/RemoveTrigger.hpp>
#include <ClipLauncher/CellModel.hpp>
SCORE_COMMAND_DECL_T(Scenario::Command::AddTrigger<ClipLauncher::CellModel>)
SCORE_COMMAND_DECL_T(Scenario::Command::RemoveTrigger<ClipLauncher::CellModel>)
