#pragma once
#include <vector>

namespace iscore
{
struct DocumentContext;
class Command;
}

// This rollback only undoes creational commands as an optimization
struct ScenarioRollbackStrategy
{
  static void rollback(
      const iscore::DocumentContext& ctx,
      const std::vector<iscore::Command*>& cmds);
};
