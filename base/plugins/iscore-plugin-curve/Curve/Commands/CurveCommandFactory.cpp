#include "CurveCommandFactory.hpp"
#include <iscore/command/Command.hpp>

namespace Curve
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Curve"};
  return key;
}
}
