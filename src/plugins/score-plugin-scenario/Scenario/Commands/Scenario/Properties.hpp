#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/PropertyCommand.hpp>

namespace Scenario::Command
{
using ProcessModel = Scenario::ProcessModel;
}
PROPERTY_COMMAND_T(
    Scenario::Command, SetScenarioExclusive, ProcessModel::p_exclusive,
    "Change exclusive mode")
SCORE_COMMAND_DECL_T(Scenario::Command::SetScenarioExclusive)
