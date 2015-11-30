#include "AutomationCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& AutomationCommandFactoryName() {
    static const CommandParentFactoryKey key{"Automation"};
    return key;
}
