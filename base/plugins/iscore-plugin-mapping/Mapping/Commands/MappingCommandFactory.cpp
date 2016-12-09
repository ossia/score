#include "MappingCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandParentFactoryKey& MappingCommandFactoryName()
{
  static const CommandParentFactoryKey key{"Mapping"};
  return key;
}
