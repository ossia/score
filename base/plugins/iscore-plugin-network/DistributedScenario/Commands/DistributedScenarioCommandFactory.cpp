#include "DistributedScenarioCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

namespace Network
{
namespace Command
{
const CommandParentFactoryKey& DistributedScenarioCommandFactoryName() {
    static const CommandParentFactoryKey key{"DistributedScenario"};
    return key;
}
}
}
