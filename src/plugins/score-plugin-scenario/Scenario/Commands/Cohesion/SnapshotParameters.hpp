#pragma once
#include <score_plugin_scenario_export.h>

namespace score
{
struct DocumentContext;
}
namespace Scenario
{
SCORE_PLUGIN_SCENARIO_EXPORT void SnapshotParametersInStates(const score::DocumentContext& doc);
}
