#pragma once
class BaseScenario;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
namespace iscore {
class CommandStackFacade;
}  // namespace iscore

namespace Scenario
{
void removeSelection(const Scenario::ScenarioModel&, iscore::CommandStackFacade&);
void clearContentFromSelection(const Scenario::ScenarioModel&, iscore::CommandStackFacade&);
void removeSelection(const BaseScenario&, iscore::CommandStackFacade&);
void clearContentFromSelection(const BaseScenario&, iscore::CommandStackFacade&);
}
