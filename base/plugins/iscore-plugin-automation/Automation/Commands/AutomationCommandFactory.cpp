#include "AutomationCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

namespace Automation
{
const CommandParentFactoryKey& CommandFactoryName() {
    static const CommandParentFactoryKey key{"Automation"};
    return key;
}
}
