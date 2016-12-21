#include "MappingCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandGroupKey& MappingCommandFactoryName()
{
  static const CommandGroupKey key{"Mapping"};
  return key;
}
