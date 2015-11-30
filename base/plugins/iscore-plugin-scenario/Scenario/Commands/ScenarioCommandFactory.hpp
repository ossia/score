#pragma once
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& ScenarioCommandFactoryName();

namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

template<>
const CommandParentFactoryKey& CommandFactoryName<Scenario::ScenarioModel>();

class BaseScenario;

template<>
const CommandParentFactoryKey& CommandFactoryName<BaseScenario>();
