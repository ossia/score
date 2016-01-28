#include "CurveCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

namespace Curve
{
const CommandParentFactoryKey& CommandFactoryName() {
    static const CommandParentFactoryKey key{"Curve"};
    return key;
}
}
