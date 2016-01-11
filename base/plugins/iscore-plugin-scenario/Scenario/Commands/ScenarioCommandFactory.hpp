#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore_plugin_scenario_export.h>


namespace Scenario
{
class BaseScenario;
class ScenarioModel;
namespace Command
{

ISCORE_PLUGIN_SCENARIO_EXPORT const CommandParentFactoryKey& ScenarioCommandFactoryName();


}

}  // namespace Scenario

template<>
const CommandParentFactoryKey& CommandFactoryName<Scenario::ScenarioModel>();


template<>
const CommandParentFactoryKey& CommandFactoryName<Scenario::BaseScenario>();
