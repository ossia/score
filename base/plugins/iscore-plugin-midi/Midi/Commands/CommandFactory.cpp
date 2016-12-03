#include "CommandFactory.hpp"

namespace Midi
{
const CommandParentFactoryKey& CommandFactoryName()
{
  static const CommandParentFactoryKey key{"Midi"};
  return key;
}
}
