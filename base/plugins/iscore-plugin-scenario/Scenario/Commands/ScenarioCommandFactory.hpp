#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore_plugin_scenario_export.h>

ISCORE_PLUGIN_SCENARIO_EXPORT const CommandParentFactoryKey& ScenarioCommandFactoryName();

namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

template<>
const CommandParentFactoryKey& CommandFactoryName<Scenario::ScenarioModel>();

class BaseScenario;

template<>
const CommandParentFactoryKey& CommandFactoryName<BaseScenario>();
