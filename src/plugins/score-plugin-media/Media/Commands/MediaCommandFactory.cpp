#include "MediaCommandFactory.hpp"
namespace Media
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Media"};
  return key;
}
}
