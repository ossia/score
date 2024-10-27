#pragma once
#include <State/Expression.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>

namespace Scenario::Command
{
using TimeSyncModel = Scenario::TimeSyncModel;

}

PROPERTY_COMMAND_T(
    Scenario::Command, SetTimeSyncMusicalSync, TimeSyncModel::p_musicalSync, "Set sync")
SCORE_COMMAND_DECL_T(Scenario::Command::SetTimeSyncMusicalSync)

PROPERTY_COMMAND_T(
    Scenario::Command, SetTimeSyncIsActive, TimeSyncModel::p_active, "Set active")
SCORE_COMMAND_DECL_T(Scenario::Command::SetTimeSyncIsActive)

PROPERTY_COMMAND_T(
    Scenario::Command, SetTimeSyncIsStartPoint, TimeSyncModel::p_startPoint,
    "Set start point")
SCORE_COMMAND_DECL_T(Scenario::Command::SetTimeSyncIsStartPoint)

PROPERTY_COMMAND_T(
    Scenario::Command, SetTimeSyncIsAutoTrigger, TimeSyncModel::p_autotrigger,
    "Set auto-trigger")
SCORE_COMMAND_DECL_T(Scenario::Command::SetTimeSyncIsAutoTrigger)
