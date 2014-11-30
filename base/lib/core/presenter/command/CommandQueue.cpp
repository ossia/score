#include <core/presenter/command/CommandQueue.hpp>
#include <core/presenter/command/SerializableCommand.hpp>
#include <QDebug>
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
	qDebug(Q_FUNC_INFO);
	qDebug() << this->count();
	emit push_start(cmd);
	QUndoStack::push(cmd);
}
