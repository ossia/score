// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceExplorerCommandFactory.hpp"

#include <score/command/Command.hpp>

namespace Explorer
{
namespace Command
{
const CommandGroupKey& DeviceExplorerCommandFactoryName()
{
  static const CommandGroupKey key{"DeviceExplorerApplicationPlugin"};
  return key;
}
}
}
