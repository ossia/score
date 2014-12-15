#include "DeleteConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"


using namespace iscore;


EmptyConstraintBox::EmptyConstraintBox(ObjectPath&& constraintPath):
	iscore::SerializableCommand{"ScenarioControl",
								"EmptyConstraintBox",
								QObject::tr("Clear a box")},
	m_path{std::move(constraintPath)}
{

	// Sérializer la Box de la contrainte.
}

void EmptyConstraintBox::undo()
{
}

void EmptyConstraintBox::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

}

int EmptyConstraintBox::id() const
{
	return 1;
}

bool EmptyConstraintBox::mergeWith(const QUndoCommand* other)
{
	return false;
}


// TODO
void EmptyConstraintBox::serializeImpl(QDataStream&)
{
}

void EmptyConstraintBox::deserializeImpl(QDataStream&)
{
}
