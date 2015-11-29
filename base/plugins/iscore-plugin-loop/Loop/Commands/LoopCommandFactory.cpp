#include "LoopCommandFactory.hpp"
#include "iscore/command/SerializableCommand.hpp"

const CommandParentFactoryKey& LoopCommandFactoryName() {
    static const CommandParentFactoryKey key{"Loop"};
    return key;
}

namespace Loop {
class ProcessModel;
}  // namespace Loop

template<>
const CommandParentFactoryKey& CommandFactoryName<Loop::ProcessModel>()
{ return LoopCommandFactoryName(); }
