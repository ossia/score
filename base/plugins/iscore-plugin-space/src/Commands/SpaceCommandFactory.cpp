#include "SpaceCommandFactory.hpp"
namespace Space
{
const CommandParentFactoryKey& CommandFactoryName(){
    static const CommandParentFactoryKey key{"SpaceApplicationPlugin"};
    return key;
}
}

