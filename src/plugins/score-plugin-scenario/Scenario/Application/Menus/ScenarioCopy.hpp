#pragma once
#include <score_plugin_scenario_export.h>

#include <vector>
class QJsonObject;
class QObject;
class Selection;
namespace score
{
struct DocumentContext;
}
namespace Process
{
class ProcessModel;
}

namespace Scenario
{
class IntervalModel;
class ProcessModel;
class EventModel;
class StateModel;
class TimeSyncModel;

class ScenarioInterface;
class BaseScenario;
class BaseScenarioContainer;

struct SCORE_PLUGIN_SCENARIO_EXPORT CategorisedScenario
{
  CategorisedScenario();
  CategorisedScenario(const Scenario::ProcessModel& sm);
  CategorisedScenario(const BaseScenarioContainer& sm);
  CategorisedScenario(const ScenarioInterface& sm);
  CategorisedScenario(const Selection& sm);

  std::vector<const IntervalModel*> selectedIntervals;
  std::vector<const EventModel*> selectedEvents;
  std::vector<const StateModel*> selectedStates;
  std::vector<const TimeSyncModel*> selectedTimeSyncs;
};

QJsonObject copyBaseInterval(const IntervalModel&);

SCORE_PLUGIN_SCENARIO_EXPORT
QJsonObject copySelectedScenarioElements(const Scenario::ProcessModel& sm);

SCORE_PLUGIN_SCENARIO_EXPORT
QJsonObject copyWholeScenario(const Scenario::ProcessModel& sm);

SCORE_PLUGIN_SCENARIO_EXPORT
QJsonObject copySelectedScenarioElements(
    const Scenario::ProcessModel& sm,
    CategorisedScenario& cat);

SCORE_PLUGIN_SCENARIO_EXPORT
QJsonObject copyProcess(const Process::ProcessModel&);

/**
 * The parent should be in the object tree of the scenario.
 * This is because the StateModel needs acces to the command stack
 * of the document upon creation.
 *
 * TODO instead we should follow the second
 * part of this article : https://doc.qt.io/archives/qq/qq25-undo.html
 * which explains how to use a proxy model to perform the undo - redo
 * operations.
 * This proxy model should be owned by the presenters where there is an easy
 * and
 * sensical access to the command stack
 */
// QJsonObject copySelectedScenarioElements(
//        const BaseScenario& sm,
//        QObject* parent);
QJsonObject
copySelectedScenarioElements(const BaseScenarioContainer& sm, QObject* parent);

QJsonObject copySelectedElementsToJson(
    ScenarioInterface& s,
    const score::DocumentContext& ctx);
}
