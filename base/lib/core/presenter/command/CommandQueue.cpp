#include <core/presenter/command/CommandQueue.hpp>
#include <core/presenter/command/SerializableCommand.hpp>

using namespace iscore;

CommandQueue::CommandQueue()
{
	this->setObjectName("CommandQueue");
}

void CommandQueue::push(SerializableCommand* cmd)
{
	QUndoStack::push(cmd);
}

void CommandQueue::pushAndEmit(SerializableCommand* cmd)
{
	emit push_start(cmd);
	QUndoStack::push(cmd);
}
