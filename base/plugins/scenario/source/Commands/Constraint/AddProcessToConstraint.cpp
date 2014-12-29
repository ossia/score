#include "AddProcessToConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddProcessToConstraint::AddProcessToConstraint():
	SerializableCommand{"ScenarioControl",
						"AddProcessToConstraintCommand",
						QObject::tr("Add process")}
{
}

AddProcessToConstraint::AddProcessToConstraint(ObjectPath&& constraintPath, QString process):
	SerializableCommand{"ScenarioControl",
						"AddProcessToConstraintCommand",
						QObject::tr("Add process")},
	m_path{std::move(constraintPath)},
	m_processName{process}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	m_createdProcessId = getNextId(constraint->processes());
}

void AddProcessToConstraint::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	constraint->removeProcess(m_createdProcessId);
}

void AddProcessToConstraint::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	// Create process model
	constraint->createProcess(m_processName, m_createdProcessId);
}

int AddProcessToConstraint::id() const
{
	return 1;
}

bool AddProcessToConstraint::mergeWith(const QUndoCommand* other)
{
	return false;
}

// TODO idea: maybe put the data in a tuple so that it can be serialized automagically ?
void AddProcessToConstraint::serializeImpl(QDataStream& s)
{
	s << m_path << m_processName << m_createdProcessId;
}

void AddProcessToConstraint::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processName >> m_createdProcessId;
}
