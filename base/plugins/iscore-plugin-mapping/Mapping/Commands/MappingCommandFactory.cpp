#include "MappingCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& MappingCommandFactoryName() {
    static const CommandParentFactoryKey key{"Mapping"};
    return key;
}
