#include "ExecutionAction.hpp"

namespace Execution
{

ExecutionAction::~ExecutionAction() { }

void ExecutionAction::startTick(const ossia::audio_tick_state& st) { }

void ExecutionAction::endTick(const ossia::audio_tick_state& st) { }
Execution::ExecutionActionList::~ExecutionActionList() { }

}
