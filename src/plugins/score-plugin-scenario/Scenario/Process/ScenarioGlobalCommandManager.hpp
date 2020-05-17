#pragma once
#include <Scenario/Process/Algorithms/Accessors.hpp>

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
void removeSelection(const Scenario::ProcessModel&, const score::DocumentContext& ctx);
void removeSelection(const BaseScenario&, const score::DocumentContext& ctx);

// Clearing content should be available for other plug-ins, e.g. loop
SCORE_PLUGIN_SCENARIO_EXPORT void clearContentFromSelection(
    const Scenario::ScenarioInterface& iface,
    const score::DocumentContext& stack);

void mergeTimeSyncs(const Scenario::ProcessModel&, const score::CommandStackFacade&);
void mergeEvents(const Scenario::ProcessModel&, const score::CommandStackFacade&);

// MOVEME : these are useful.
template <typename T>
struct StartDateComparator
{
  const Scenario::ProcessModel* scenario;
  bool operator()(const T* lhs, const T* rhs) const
  {
    return Scenario::date(*lhs, *scenario) < Scenario::date(*rhs, *scenario);
  }
};

struct SCORE_PLUGIN_SCENARIO_EXPORT EndDateComparator
{
  const Scenario::ProcessModel* scenario;
  bool operator()(const IntervalModel* lhs, const IntervalModel* rhs) const;
};
}
