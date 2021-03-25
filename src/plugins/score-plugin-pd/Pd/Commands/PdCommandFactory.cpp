#include "PdCommandFactory.hpp"

const CommandGroupKey& Pd::CommandFactoryName()
{
  static const CommandGroupKey key{"Pd"};
  return key;
}
