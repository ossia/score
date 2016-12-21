#pragma once
#include <vector>

namespace iscore
{
class Command;
}

// This rollback only undoes creational commands as an optimization
struct ScenarioRollbackStrategy
{
  static void rollback(const std::vector<iscore::Command*>& cmds);
};
