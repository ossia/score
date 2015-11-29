#include "JSCommandFactory.hpp"
#include "iscore/command/SerializableCommand.hpp"

const CommandParentFactoryKey& JSCommandFactoryName() {
    static const CommandParentFactoryKey key{"JS"};
    return key;
}
