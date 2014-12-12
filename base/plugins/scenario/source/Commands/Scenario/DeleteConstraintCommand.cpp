#include "DeleteConstraintCommand.hpp"
using namespace iscore;


void DeleteConstraintCommand::undo()
{
}

void DeleteConstraintCommand::redo()
{
}

int DeleteConstraintCommand::id() const
{
	return 1;
}

bool DeleteConstraintCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void DeleteConstraintCommand::serializeImpl(QDataStream&)
{
}

void DeleteConstraintCommand::deserializeImpl(QDataStream&)
{
}
