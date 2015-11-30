#include "IScoreCohesionCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& IScoreCohesionCommandFactoryName() {
    static const CommandParentFactoryKey key{"Cohesion"};
    return key;
}
