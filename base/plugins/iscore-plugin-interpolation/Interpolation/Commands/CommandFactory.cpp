#include "CommandFactory.hpp"
namespace Interpolation
{
const CommandParentFactoryKey& CommandFactoryName()
{
  static const CommandParentFactoryKey key{"Interpolation"};
  return key;
}
}
