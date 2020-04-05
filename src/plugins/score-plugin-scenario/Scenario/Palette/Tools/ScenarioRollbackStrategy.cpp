// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioRollbackStrategy.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateEvent_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateSequence.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>

#include <score/command/Command.hpp>
#include <score/plugins/StringFactoryKey.hpp>

void ScenarioRollbackStrategy::rollback(
    const score::DocumentContext& ctx,
    const std::vector<score::Command*>& cmds)
{
  // TODO UPDATE THIS ELSE ROLLBACK WON'T WORK.
  // REFACTOR THIS IN A LIST SOMEWHERE.
  using namespace Scenario::Command;
  for (int i = cmds.size() - 1; i >= 0; --i)
  {
    if (cmds[i]->key() == CreateInterval::static_key()
        || cmds[i]->key() == CreateState::static_key()
        || cmds[i]->key() == CreateEvent_State::static_key()
        || cmds[i]->key() == CreateInterval_State::static_key()
        || cmds[i]->key() == CreateInterval_State_Event::static_key()
        || cmds[i]->key() == CreateInterval_State_Event_TimeSync::static_key()
        || cmds[i]->key() == CreateSequence::static_key())
    {
      cmds[i]->undo(ctx);
    }
  }
}

void DefaultRollbackStrategy::rollback(
    const score::DocumentContext& ctx,
    const std::vector<score::Command*>& cmds)
{
  for (int i = cmds.size() - 1; i >= 0; --i)
  {
    cmds[i]->undo(ctx);
  }
}
