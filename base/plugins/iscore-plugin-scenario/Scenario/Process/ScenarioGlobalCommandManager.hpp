#pragma once
#include <core/command/CommandStack.hpp>
namespace Scenario { class ScenarioModel; }
class BaseScenario;
namespace Scenario
{
void removeSelection(const Scenario::ScenarioModel&, iscore::CommandStack&);
void clearContentFromSelection(const Scenario::ScenarioModel&, iscore::CommandStack&);
void removeSelection(const BaseScenario&, iscore::CommandStack&);
void clearContentFromSelection(const BaseScenario&, iscore::CommandStack&);
}
