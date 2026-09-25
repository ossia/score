#pragma once
#include <score_plugin_scenario_export.h>

#include <vector>
namespace score
{
class CommandStackFacade;
struct DocumentContext;
}

namespace Scenario
{
class StateModel;
namespace Command
{
SCORE_PLUGIN_SCENARIO_EXPORT void RefreshStates(const score::DocumentContext& doc);
SCORE_PLUGIN_SCENARIO_EXPORT void
RefreshStates(const std::vector<const StateModel*>&, const score::DocumentContext&);
}
}
