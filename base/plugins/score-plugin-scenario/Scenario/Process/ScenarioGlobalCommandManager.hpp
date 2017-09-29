#pragma once
#include <QList>
#include <score_plugin_scenario_export.h>
class BaseScenario;
namespace Scenario
{
class IntervalModel;
class StateModel;
class ProcessModel;
class BaseScenarioContainer;
class ScenarioInterface;
} // namespace Scenario
namespace score
{
class CommandStackFacade;
} // namespace score

namespace Scenario
{
// Remove elements : only makes sense where elements can be removed,
// i.e. with a Scenario
void removeSelection(
    const Scenario::ProcessModel&, const score::CommandStackFacade&);
void removeSelection(const BaseScenario&, const score::CommandStackFacade&);

// Clearing content should be available for other plug-ins, e.g. loop
SCORE_PLUGIN_SCENARIO_EXPORT void clearContentFromSelection(
    const QList<const IntervalModel*>& intervalsToRemove,
    const QList<const StateModel*>& statesToRemove,
    const score::CommandStackFacade& stack);

SCORE_PLUGIN_SCENARIO_EXPORT void clearContentFromSelection(
    const BaseScenarioContainer&, const score::CommandStackFacade&);

void clearContentFromSelection(
    const Scenario::ScenarioInterface&, const score::CommandStackFacade&);
void clearContentFromSelection(
    const Scenario::ProcessModel&, const score::CommandStackFacade&);
void clearContentFromSelection(
    const BaseScenario&, const score::CommandStackFacade&);

void mergeTimeSyncs(
    const Scenario::ProcessModel&, const score::CommandStackFacade&);
void mergeEvents(
    const Scenario::ProcessModel&, const score::CommandStackFacade&);
}
