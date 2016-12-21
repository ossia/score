#include "CommandFactory.hpp"
namespace Interpolation
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Interpolation"};
  return key;
}
}
