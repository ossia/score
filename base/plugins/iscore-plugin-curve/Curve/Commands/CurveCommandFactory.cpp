#include "CurveCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& CurveCommandFactoryName() {
    static const CommandParentFactoryKey key{"Curve"};
    return key;
}
