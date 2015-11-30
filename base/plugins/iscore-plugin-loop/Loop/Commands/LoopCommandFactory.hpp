#pragma once
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& LoopCommandFactoryName();

namespace Loop
{ class ProcessModel; }
template<>
const CommandParentFactoryKey& CommandFactoryName<Loop::ProcessModel>();
