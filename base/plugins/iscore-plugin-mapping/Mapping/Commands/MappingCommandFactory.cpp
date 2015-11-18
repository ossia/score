#include "MappingCommandFactory.hpp"

const CommandParentFactoryKey& MappingCommandFactoryName() {
    static const CommandParentFactoryKey key{"MappingControl"};
    return key;
}
