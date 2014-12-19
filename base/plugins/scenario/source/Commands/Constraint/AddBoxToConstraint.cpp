#include "AddBoxToConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddBoxToConstraint::AddBoxToConstraint(ObjectPath&& constraintPath):
	SerializableCommand{"ScenarioControl",
						"AddBoxToConstraint",
						"Add empty box"},
	m_path{constraintPath}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	m_createdBoxId = getNextId(constraint->boxes());
}

void AddBoxToConstraint::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	constraint->removeBox(m_createdBoxId);
}

void AddBoxToConstraint::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	constraint->createBox(m_createdBoxId);
}

int AddBoxToConstraint::id() const
{
	return 1;
}

bool AddBoxToConstraint::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddBoxToConstraint::serializeImpl(QDataStream& s)
{
	s << m_path << m_createdBoxId;
}

void AddBoxToConstraint::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_createdBoxId;
}
