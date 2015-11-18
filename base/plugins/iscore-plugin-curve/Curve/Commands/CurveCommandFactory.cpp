#include "CurveCommandFactory.hpp"
const CommandParentFactoryKey& CurveCommandFactoryName() {
    static const CommandParentFactoryKey key{"CurveControl"};
    return key;
}
