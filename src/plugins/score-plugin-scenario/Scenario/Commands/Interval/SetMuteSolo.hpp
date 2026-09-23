#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/PropertyCommand.hpp>

namespace Scenario::Command
{
using IntervalModel = Scenario::IntervalModel;
}

PROPERTY_COMMAND_T(
    Scenario::Command, SetIntervalMuted, IntervalModel::p_muted, "Mute an interval")
SCORE_COMMAND_DECL_T(Scenario::Command::SetIntervalMuted)

PROPERTY_COMMAND_T(
    Scenario::Command, SetIntervalSoloed, IntervalModel::p_soloed, "Solo an interval")
SCORE_COMMAND_DECL_T(Scenario::Command::SetIntervalSoloed)
