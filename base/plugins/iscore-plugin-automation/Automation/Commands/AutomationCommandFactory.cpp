#include "AutomationCommandFactory.hpp"
#include <iscore/command/Command.hpp>

namespace Automation
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Automation"};
  return key;
}
}
