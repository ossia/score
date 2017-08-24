#pragma once
#include <QList>
#include <iscore_plugin_scenario_export.h>
class BaseScenario;
namespace Scenario
{
class ConstraintModel;
class StateModel;
class ProcessModel;
class BaseScenarioContainer;
class ScenarioInterface;
} // namespace Scenario
namespace iscore
{
class CommandStackFacade;
} // namespace iscore

namespace Scenario
{
// Remove elements : only makes sense where elements can be removed,
// i.e. with a Scenario
void removeSelection(
    const Scenario::ProcessModel&, const iscore::CommandStackFacade&);
void removeSelection(const BaseScenario&, const iscore::CommandStackFacade&);

// Clearing content should be available for other plug-ins, e.g. loop
ISCORE_PLUGIN_SCENARIO_EXPORT void clearContentFromSelection(
    const QList<const ConstraintModel*>& constraintsToRemove,
    const QList<const StateModel*>& statesToRemove,
    const iscore::CommandStackFacade& stack);

ISCORE_PLUGIN_SCENARIO_EXPORT void clearContentFromSelection(
    const BaseScenarioContainer&, const iscore::CommandStackFacade&);

void clearContentFromSelection(
    const Scenario::ScenarioInterface&, const iscore::CommandStackFacade&);
void clearContentFromSelection(
    const Scenario::ProcessModel&, const iscore::CommandStackFacade&);
void clearContentFromSelection(
    const BaseScenario&, const iscore::CommandStackFacade&);

void mergeTimeSyncs(
    const Scenario::ProcessModel&, const iscore::CommandStackFacade&);
}
