#pragma once
#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>
#include <Loop/LoopProcessModel.hpp>

ISCORE_COMMAND_DECL_T(AddTrigger<Loop::ProcessModel>)
ISCORE_COMMAND_DECL_T(RemoveTrigger<Loop::ProcessModel>)
