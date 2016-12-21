#include "ScenarioCommandFactory.hpp"
#include <iscore/command/Command.hpp>

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
