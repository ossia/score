#pragma once
class BaseScenario;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
namespace iscore {
class CommandStack;
}  // namespace iscore

namespace Scenario
{
void removeSelection(const Scenario::ScenarioModel&, iscore::CommandStack&);
void clearContentFromSelection(const Scenario::ScenarioModel&, iscore::CommandStack&);
void removeSelection(const BaseScenario&, iscore::CommandStack&);
void clearContentFromSelection(const BaseScenario&, iscore::CommandStack&);
}
