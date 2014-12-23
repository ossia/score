#include "AddProcessToConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"

using namespace iscore;

AddProcessToConstraintCommand::AddProcessToConstraintCommand(ObjectPath&& constraintPath, QString process):
	SerializableCommand{"ScenarioControl",
						"AddProcessToConstraintCommand",
						"Add process"},
	m_path(std::move(constraintPath)),
	m_processName{process}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	m_createdProcessId = getNextId(constraint->processes());
}

void AddProcessToConstraintCommand::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	constraint->removeProcess(m_createdProcessId);
}

void AddProcessToConstraintCommand::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	// Create process model
	constraint->createProcess(m_processName, m_createdProcessId);
}

int AddProcessToConstraintCommand::id() const
{
	return 1;
}

bool AddProcessToConstraintCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

// TODO idea: maybe put the data in a tuple so that it can be serialized automagically ?
void AddProcessToConstraintCommand::serializeImpl(QDataStream& s)
{
	s << m_path << m_processName << m_createdProcessId;
}

void AddProcessToConstraintCommand::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processName >> m_createdProcessId;
}
