#include "MappingCommandFactory.hpp"

const CommandParentFactoryKey& MappingCommandFactoryName() {
    static const CommandParentFactoryKey key{"Mapping"};
    return key;
}
