// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Loop/LoopProcessModel.hpp>
#include <QByteArray>
#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include "LoopTriggerCommandFactory.hpp"
#include <Scenario/Process/ScenarioInterface.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

bool LoopTriggerCommandFactory::matches(
    const Scenario::TimeNodeModel& tn) const
{
  return dynamic_cast<Loop::ProcessModel*>(tn.parent());
}

iscore::Command* LoopTriggerCommandFactory::make_addTriggerCommand(
    const Scenario::TimeNodeModel& tn) const
{
  if (dynamic_cast<Loop::ProcessModel*>(tn.parent()))
  {
    return new Scenario::Command::AddTrigger<Loop::ProcessModel>(tn);
  }
  return nullptr;
}

iscore::Command*
LoopTriggerCommandFactory::make_removeTriggerCommand(
    const Scenario::TimeNodeModel& tn) const
{
  if (dynamic_cast<Loop::ProcessModel*>(tn.parent()))
  {
    return new Scenario::Command::RemoveTrigger<Loop::ProcessModel>(tn);
  }
  return nullptr;
}
