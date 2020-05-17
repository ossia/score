#pragma once
#include <score_plugin_scenario_export.h>

#include <vector>

namespace score
{
struct DocumentContext;
class Command;
}

// This rollback only undoes creational commands as an optimization
struct ScenarioRollbackStrategy
{
  static void
  rollback(const score::DocumentContext& ctx, const std::vector<score::Command*>& cmds);
};

struct SCORE_PLUGIN_SCENARIO_EXPORT DefaultRollbackStrategy
{
  static void
  rollback(const score::DocumentContext& ctx, const std::vector<score::Command*>& cmds);
};
