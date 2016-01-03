#include "SpaceCommandFactory.hpp"
namespace Space
{
const CommandParentFactoryKey& SpaceCommandFactoryName(){
    static const CommandParentFactoryKey key{"SpaceApplicationPlugin"};
    return key;
}
}

