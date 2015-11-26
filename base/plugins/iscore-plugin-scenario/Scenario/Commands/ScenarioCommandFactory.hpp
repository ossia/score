#pragma once
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& ScenarioCommandFactoryName();


namespace Scenario { class ScenarioModel; }
template<>
inline const CommandParentFactoryKey& CommandFactoryName<Scenario::ScenarioModel>()
{ return ScenarioCommandFactoryName(); }

class BaseScenario;
template<>
inline const CommandParentFactoryKey& CommandFactoryName<BaseScenario>()
{ return ScenarioCommandFactoryName(); }
