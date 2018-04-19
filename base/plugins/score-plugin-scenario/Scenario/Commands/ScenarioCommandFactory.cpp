// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioCommandFactory.hpp"

#include <score/command/Command.hpp>

namespace Scenario
{
namespace Command
{

const CommandGroupKey& ScenarioCommandFactoryName()
{
  static const CommandGroupKey key{"ScenarioApplicationPlugin"};
  return key;
}
}
}

template <>
const CommandGroupKey& CommandFactoryName<Scenario::ProcessModel>()
{
  return Scenario::Command::ScenarioCommandFactoryName();
}

template <>
const CommandGroupKey& CommandFactoryName<Scenario::BaseScenario>()
{
  return Scenario::Command::ScenarioCommandFactoryName();
}
