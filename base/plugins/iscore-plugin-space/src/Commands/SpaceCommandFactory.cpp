#include "SpaceCommandFactory.hpp"
const CommandParentFactoryKey& SpaceCommandFactoryName(){
    static const CommandParentFactoryKey key{"SpaceControl"};
    return key;
}

