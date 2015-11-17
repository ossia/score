#include "LoopCommandFactory.hpp"
const CommandParentFactoryKey& LoopCommandFactoryName() {
    static const CommandParentFactoryKey key{"Loop"};
    return key;
}
