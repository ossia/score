#include "AutomationCommandFactory.hpp"

const CommandParentFactoryKey& AutomationCommandFactoryName() {
    static const CommandParentFactoryKey key{"Automation"};
    return key;
}
