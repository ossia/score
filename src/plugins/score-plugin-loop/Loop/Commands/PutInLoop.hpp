#pragma once
#include <Loop/Commands/LoopCommandFactory.hpp>
#include <Scenario/Commands/Scenario/Encapsulate.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Loop
{

class SCORE_PLUGIN_LOOP_EXPORT PutInLoop final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(LoopCommandFactoryName(), PutInLoop, "Put in loop")
};

void EncapsulateInLoop(const Scenario::ProcessModel& loop, const score::CommandStackFacade& stack);
}
