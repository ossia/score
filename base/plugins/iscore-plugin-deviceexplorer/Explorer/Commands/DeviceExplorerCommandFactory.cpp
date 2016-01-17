#include "DeviceExplorerCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>


namespace DeviceExplorer
{
namespace Command
{
const CommandParentFactoryKey& DeviceExplorerCommandFactoryName() {
    static const CommandParentFactoryKey key{"DeviceExplorerApplicationPlugin"};
    return key;
}

}
}
