#include "LoopCommandFactory.hpp"
const CommandParentFactoryKey& LoopCommandFactoryName() {
    static const CommandParentFactoryKey key{"Loop"};
    return key;
}

namespace Loop { class ProcessModel; }
template<>
const CommandParentFactoryKey& CommandFactoryName<Loop::ProcessModel>()
{ return LoopCommandFactoryName(); }
