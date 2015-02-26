#include <core/presenter/command/CommandQueue.hpp>
#include <core/presenter/command/SerializableCommand.hpp>
#include <QDebug>
using namespace iscore;

CommandQueue::CommandQueue (QObject* parent) :
    QUndoStack {nullptr}
{
    this->setObjectName ("CommandQueue");
    this->setParent (parent);
}

void CommandQueue::push (SerializableCommand* cmd)
{
    QUndoStack::push (cmd);
}

void CommandQueue::pushAndEmit (SerializableCommand* cmd)
{
    emit push_start (cmd);
    QUndoStack::push (cmd);
}
