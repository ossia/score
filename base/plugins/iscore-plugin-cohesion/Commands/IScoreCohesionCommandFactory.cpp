#include "IScoreCohesionCommandFactory.hpp"

const CommandParentFactoryKey& IScoreCohesionCommandFactoryName() {
    static const CommandParentFactoryKey key{"CohesionControl"};
    return key;
}
