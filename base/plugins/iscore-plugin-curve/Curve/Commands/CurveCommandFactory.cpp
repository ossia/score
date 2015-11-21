#include "CurveCommandFactory.hpp"
const CommandParentFactoryKey& CurveCommandFactoryName() {
    static const CommandParentFactoryKey key{"Curve"};
    return key;
}
