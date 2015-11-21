#include "SpaceCommandFactory.hpp"
const CommandParentFactoryKey& SpaceCommandFactoryName(){
    static const CommandParentFactoryKey key{"SpaceApplicationPlugin"};
    return key;
}

