#pragma once
#include <score/command/Command.hpp>

#include <score_plugin_loop_export.h>

const CommandGroupKey& LoopCommandFactoryName();

namespace Loop
{
class ProcessModel;
}
template <>
SCORE_PLUGIN_LOOP_EXPORT const CommandGroupKey& CommandFactoryName<Loop::ProcessModel>();
