// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationCommandFactory.hpp"

#include <score/command/Command.hpp>

namespace Automation
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Automation"};
  return key;
}
}
