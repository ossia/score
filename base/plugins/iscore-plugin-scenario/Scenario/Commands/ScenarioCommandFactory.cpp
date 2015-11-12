#include "ScenarioCommandFactory.hpp"
const CommandParentFactoryKey& ScenarioCommandFactoryName(){
    static const CommandParentFactoryKey key{"ScenarioControl"};
    return key;
}
