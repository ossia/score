#include "DeviceExplorerCommandFactory.hpp"
#include <iscore/command/Command.hpp>

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
