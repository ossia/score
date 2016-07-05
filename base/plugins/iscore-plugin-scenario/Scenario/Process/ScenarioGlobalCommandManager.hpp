#pragma once
class BaseScenario;
namespace Scenario {
class ProcessModel;
}  // namespace Scenario
namespace iscore {
class CommandStackFacade;
}  // namespace iscore

namespace Scenario
{
void removeSelection(const Scenario::ProcessModel&, const iscore::CommandStackFacade&);
void clearContentFromSelection(const Scenario::ProcessModel&, const iscore::CommandStackFacade&);
void removeSelection(const BaseScenario&, const iscore::CommandStackFacade&);
void clearContentFromSelection(const BaseScenario&, const iscore::CommandStackFacade&);

void mergeTimeNodes(const Scenario::ProcessModel&, const iscore::CommandStackFacade&);
}
