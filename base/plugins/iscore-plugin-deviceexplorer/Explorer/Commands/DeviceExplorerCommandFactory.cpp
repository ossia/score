#include "DeviceExplorerCommandFactory.hpp"

const CommandParentFactoryKey& DeviceExplorerCommandFactoryName() {
    static const CommandParentFactoryKey key{"DeviceExplorerControl"};
    return key;
}
