#include "AutomationCommandFactory.hpp"

const CommandParentFactoryKey& AutomationCommandFactoryName() {
    static const CommandParentFactoryKey key{"AutomationControl"};
    return key;
}
