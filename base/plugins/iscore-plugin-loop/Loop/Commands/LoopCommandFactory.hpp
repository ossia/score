#pragma once
#include <iscore/command/Command.hpp>
#include <iscore_plugin_loop_export.h>

const CommandGroupKey& LoopCommandFactoryName();

namespace Loop
{
class ProcessModel;
}
template <>
ISCORE_PLUGIN_LOOP_EXPORT const CommandGroupKey&
CommandFactoryName<Loop::ProcessModel>();
