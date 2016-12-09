#include "JSCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandParentFactoryKey& JS::CommandFactoryName()
{
  static const CommandParentFactoryKey key{"JS"};
  return key;
}
