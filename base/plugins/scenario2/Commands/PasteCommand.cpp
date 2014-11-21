#include "PasteCommand.hpp"
using namespace iscore;


void PasteCommand::undo()
{
}

void PasteCommand::redo()
{
}

int PasteCommand::id() const
{
}

bool PasteCommand::mergeWith(const QUndoCommand* other)
{
}

void PasteCommand::serializeImpl(QDataStream&)
{
}

void PasteCommand::deserializeImpl(QDataStream&)
{
}
