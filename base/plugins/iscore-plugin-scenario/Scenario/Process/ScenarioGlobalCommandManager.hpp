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
void removeSelection(const Scenario::ScenarioModel&, const iscore::CommandStackFacade&);
void clearContentFromSelection(const Scenario::ScenarioModel&, const iscore::CommandStackFacade&);
void removeSelection(const BaseScenario&, const iscore::CommandStackFacade&);
void clearContentFromSelection(const BaseScenario&, const iscore::CommandStackFacade&);
}
