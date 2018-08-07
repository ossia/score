#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Automation/AutomationModel.hpp>
#include <Color/GradientModel.hpp>
#include <State/Unit.hpp>
#include <score/command/PropertyCommand.hpp>

PROPERTY_COMMAND_T(Automation, SetMin, ProcessModel::p_min, "Set minimum")
PROPERTY_COMMAND_T(Automation, SetMax, ProcessModel::p_max, "Set maximum")
PROPERTY_COMMAND_T(Automation, SetTween, ProcessModel::p_tween, "Set tween")
PROPERTY_COMMAND_T(Automation, SetUnit, ProcessModel::p_unit, "Set unit")

SCORE_COMMAND_DECL_T(Automation::SetMin)
SCORE_COMMAND_DECL_T(Automation::SetMax)
SCORE_COMMAND_DECL_T(Automation::SetTween)
SCORE_COMMAND_DECL_T(Automation::SetUnit)

PROPERTY_COMMAND_T(Gradient, SetGradientTween, ProcessModel::p_tween, "Set tween")
SCORE_COMMAND_DECL_T(Gradient::SetGradientTween)
