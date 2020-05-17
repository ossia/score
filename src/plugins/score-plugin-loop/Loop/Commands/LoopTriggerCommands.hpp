#pragma once
#include <Loop/LoopProcessModel.hpp>
#include <Scenario/Commands/TimeSync/AddTrigger.hpp>
#include <Scenario/Commands/TimeSync/RemoveTrigger.hpp>

#include <score/serialization/DataStreamVisitor.hpp>

SCORE_COMMAND_DECL_T(Scenario::Command::AddTrigger<Loop::ProcessModel>)
SCORE_COMMAND_DECL_T(Scenario::Command::RemoveTrigger<Loop::ProcessModel>)
