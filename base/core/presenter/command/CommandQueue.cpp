#include <core/presenter/command/CommandQueue.hpp>
#include <core/presenter/command/Command.hpp>

using namespace iscore;

CommandQueue::CommandQueue()
{
	this->setObjectName("CommandQueue");
}

void CommandQueue::push(Command* cmd)
{
	QUndoStack::push(cmd);
}

void CommandQueue::pushAndEmit(Command* cmd)
{
	emit push_start(cmd);
	QUndoStack::push(cmd);
}
