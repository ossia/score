#include "CellTriggerCommandFactory.hpp"

#include <Scenario/Commands/TimeSync/AddTrigger.hpp>
#include <Scenario/Commands/TimeSync/RemoveTrigger.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/CommandFactory.hpp>

#include <score/model/path/PathSerialization.hpp>

// Instantiate AddTrigger/RemoveTrigger for CellModel
template class Scenario::Command::AddTrigger<ClipLauncher::CellModel>;
template class Scenario::Command::RemoveTrigger<ClipLauncher::CellModel>;

namespace ClipLauncher
{

bool CellTriggerCommandFactory::matches(const Scenario::TimeSyncModel& tn) const
{
  return dynamic_cast<CellModel*>(tn.parent()) != nullptr;
}

score::Command*
CellTriggerCommandFactory::make_addTriggerCommand(const Scenario::TimeSyncModel& tn) const
{
  return new Scenario::Command::AddTrigger<CellModel>{tn};
}

score::Command*
CellTriggerCommandFactory::make_removeTriggerCommand(const Scenario::TimeSyncModel& tn) const
{
  return new Scenario::Command::RemoveTrigger<CellModel>{Path<Scenario::TimeSyncModel>{tn}};
}

} // namespace ClipLauncher
