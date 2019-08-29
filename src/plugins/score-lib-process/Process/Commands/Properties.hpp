#pragma once
#include <Process/Process.hpp>
#include <Process/Commands/ProcessCommandFactory.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Process/TimeValueSerialization.hpp>

PROPERTY_COMMAND_T(
    Process,
    SetLoop,
    ProcessModel::p_loops,
    "Set process looping")
SCORE_COMMAND_DECL_T(Process::SetLoop)
PROPERTY_COMMAND_T(
    Process,
    SetStartOffset,
    ProcessModel::p_startOffset,
    "Set start offset")
SCORE_COMMAND_DECL_T(Process::SetStartOffset)
PROPERTY_COMMAND_T(
    Process,
    SetLoopDuration,
    ProcessModel::p_loopDuration,
    "Set loop duration")
SCORE_COMMAND_DECL_T(Process::SetLoopDuration)
