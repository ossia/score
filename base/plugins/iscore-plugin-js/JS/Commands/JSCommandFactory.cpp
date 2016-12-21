#include "JSCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandGroupKey& JS::CommandFactoryName()
{
  static const CommandGroupKey key{"JS"};
  return key;
}
