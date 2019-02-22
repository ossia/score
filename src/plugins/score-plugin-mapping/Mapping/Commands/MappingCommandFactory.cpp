// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MappingCommandFactory.hpp"

#include <score/command/Command.hpp>

const CommandGroupKey& MappingCommandFactoryName()
{
  static const CommandGroupKey key{"Mapping"};
  return key;
}
