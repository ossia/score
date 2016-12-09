#include "CommandFactory.hpp"

namespace Midi
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Midi"};
  return key;
}
}
