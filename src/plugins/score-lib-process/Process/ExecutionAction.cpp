#include "ExecutionAction.hpp"

namespace Execution
{

ExecutionAction::~ExecutionAction() { }

void ExecutionAction::startTick(unsigned long frameCount, double seconds) { }

void ExecutionAction::endTick(unsigned long frameCount, double seconds) { }
Execution::ExecutionActionList::~ExecutionActionList() { }

}
