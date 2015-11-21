#include "DeviceExplorerCommandFactory.hpp"

const CommandParentFactoryKey& DeviceExplorerCommandFactoryName() {
    static const CommandParentFactoryKey key{"DeviceExplorerApplicationPlugin"};
    return key;
}
