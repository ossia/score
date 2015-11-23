#pragma once
#include <core/command/CommandStack.hpp>
class ScenarioModel;
class BaseScenario;
namespace Scenario
{
void removeSelection(const ScenarioModel&, iscore::CommandStack&);
void clearContentFromSelection(const ScenarioModel&, iscore::CommandStack&);
void removeSelection(const BaseScenario&, iscore::CommandStack&);
void clearContentFromSelection(const BaseScenario&, iscore::CommandStack&);
}
