#include "DistributedScenarioCommandFactory.hpp"
const CommandParentFactoryKey&DistributedScenarioCommandFactoryName() {
    static const CommandParentFactoryKey key{"DistributedScenario"};
    return key;
}
