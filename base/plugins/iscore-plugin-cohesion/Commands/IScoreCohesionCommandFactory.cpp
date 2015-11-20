#include "IScoreCohesionCommandFactory.hpp"

const CommandParentFactoryKey& IScoreCohesionCommandFactoryName() {
    static const CommandParentFactoryKey key{"Cohesion"};
    return key;
}
