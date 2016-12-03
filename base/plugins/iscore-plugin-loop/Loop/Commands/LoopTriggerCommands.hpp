#pragma once
#include <Loop/LoopProcessModel.hpp>
#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>

ISCORE_COMMAND_DECL_T(Scenario::Command::AddTrigger<Loop::ProcessModel>)
ISCORE_COMMAND_DECL_T(Scenario::Command::RemoveTrigger<Loop::ProcessModel>)
