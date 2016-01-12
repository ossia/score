#include "ScenarioCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

namespace Scenario
{
namespace Command
{

const CommandParentFactoryKey& ScenarioCommandFactoryName(){
    static const CommandParentFactoryKey key{"ScenarioApplicationPlugin"};
    return key;
}


}
}


template<>
const CommandParentFactoryKey& CommandFactoryName<Scenario::ScenarioModel>()
{ return Scenario::Command::ScenarioCommandFactoryName(); }


template<>
const CommandParentFactoryKey& CommandFactoryName<Scenario::BaseScenario>()
{ return Scenario::Command::ScenarioCommandFactoryName(); }
