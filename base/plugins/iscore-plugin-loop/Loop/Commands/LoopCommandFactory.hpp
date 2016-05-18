#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore_plugin_loop_export.h>

const CommandParentFactoryKey& LoopCommandFactoryName();

namespace Loop
{ class ProcessModel; }
template<>
ISCORE_PLUGIN_LOOP_EXPORT const CommandParentFactoryKey& CommandFactoryName<Loop::ProcessModel>();
