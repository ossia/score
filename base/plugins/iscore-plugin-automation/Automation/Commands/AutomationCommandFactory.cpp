#include "AutomationCommandFactory.hpp"
#include <iscore/command/Command.hpp>

namespace Automation
{
const CommandParentFactoryKey& CommandFactoryName()
{
  static const CommandParentFactoryKey key{"Automation"};
  return key;
}
}
