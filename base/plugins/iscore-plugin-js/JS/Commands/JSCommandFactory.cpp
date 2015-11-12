#include "JSCommandFactory.hpp"
const CommandParentFactoryKey& JSCommandFactoryName() {
    static const CommandParentFactoryKey key{"JS"};
    return key;
}
