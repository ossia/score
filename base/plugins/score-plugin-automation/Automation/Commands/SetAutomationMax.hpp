#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/Color/GradientAutomModel.hpp>
#include <State/Unit.hpp>
#include <score/command/PropertyCommand.hpp>

namespace Automation
{
PROPERTY_COMMAND_T(SetMin, Automation::ProcessModel::p_min, "Set minimum")
PROPERTY_COMMAND_T(SetMax, Automation::ProcessModel::p_max, "Set maximum")
PROPERTY_COMMAND_T(SetTween, Automation::ProcessModel::p_tween, "Set tween")
PROPERTY_COMMAND_T(SetUnit, Automation::ProcessModel::p_unit, "Set unit")
}
SCORE_COMMAND_DECL_T(Automation::SetMin)
SCORE_COMMAND_DECL_T(Automation::SetMax)
SCORE_COMMAND_DECL_T(Automation::SetTween)
SCORE_COMMAND_DECL_T(Automation::SetUnit)

namespace Gradient
{
using Automation::CommandFactoryName;
PROPERTY_COMMAND_T(SetTween, Gradient::ProcessModel::p_tween, "Set tween")
}
SCORE_COMMAND_DECL_T(Gradient::SetTween)
