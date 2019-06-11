// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioCommandFactory.hpp"

#include <score/command/Command.hpp>

namespace Scenario
{
namespace Command
{

const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"ScenarioApplicationPlugin"};
  return key;
}
}
}

template <>
const CommandGroupKey& CommandFactoryName<Scenario::ProcessModel>()
{
  return Scenario::Command::CommandFactoryName();
}

template <>
const CommandGroupKey& CommandFactoryName<Scenario::BaseScenario>()
{
  return Scenario::Command::CommandFactoryName();
}
