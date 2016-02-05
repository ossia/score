#include "DeviceExplorerCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>


namespace Explorer
{
namespace Command
{
const CommandParentFactoryKey& DeviceExplorerCommandFactoryName() {
    static const CommandParentFactoryKey key{"DeviceExplorerApplicationPlugin"};
    return key;
}

}
}
