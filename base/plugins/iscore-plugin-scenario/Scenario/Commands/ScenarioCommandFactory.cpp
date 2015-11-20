#include "ScenarioCommandFactory.hpp"
const CommandParentFactoryKey& ScenarioCommandFactoryName(){
    static const CommandParentFactoryKey key{"ScenarioApplicationPlugin"};
    return key;
}
