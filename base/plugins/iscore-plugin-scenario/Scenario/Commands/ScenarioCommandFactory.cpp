#include "ScenarioCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& ScenarioCommandFactoryName(){
    static const CommandParentFactoryKey key{"ScenarioApplicationPlugin"};
    return key;
}


namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

template<>
const CommandParentFactoryKey& CommandFactoryName<Scenario::ScenarioModel>()
{ return ScenarioCommandFactoryName(); }

class BaseScenario;

template<>
const CommandParentFactoryKey& CommandFactoryName<BaseScenario>()
{ return ScenarioCommandFactoryName(); }
