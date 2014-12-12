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
	return 1;
}

bool PasteCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void PasteCommand::serializeImpl(QDataStream&)
{
}

void PasteCommand::deserializeImpl(QDataStream&)
{
}
