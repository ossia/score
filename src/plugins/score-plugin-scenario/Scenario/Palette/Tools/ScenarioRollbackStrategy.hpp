#pragma once
#include <vector>

namespace score
{
struct DocumentContext;
class Command;
}

// This rollback only undoes creational commands as an optimization
struct ScenarioRollbackStrategy
{
  static void rollback(
      const score::DocumentContext& ctx,
      const std::vector<score::Command*>& cmds);
};

struct DefaultRollbackStrategy
{
  static void rollback(
      const score::DocumentContext& ctx,
      const std::vector<score::Command*>& cmds);
};
