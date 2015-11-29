#include "DeviceExplorerCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>
const CommandParentFactoryKey& DeviceExplorerCommandFactoryName() {
    static const CommandParentFactoryKey key{"DeviceExplorerApplicationPlugin"};
    return key;
}
