#include "DistributedScenarioCommandFactory.hpp"
#include "iscore/command/SerializableCommand.hpp"

const CommandParentFactoryKey&DistributedScenarioCommandFactoryName() {
    static const CommandParentFactoryKey key{"DistributedScenario"};
    return key;
}
