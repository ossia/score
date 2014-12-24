#include "DeleteProcessFromConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <QDebug>

using namespace iscore;


RemoveProcessFromConstraintCommand::RemoveProcessFromConstraintCommand(ObjectPath&& constraintPath, int processId):
	SerializableCommand{"ScenarioControl",
						"DeleteProcessFromConstraintCommand",
						"Delete process"},
	m_path{std::move(constraintPath)},
	m_processId{processId}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	auto process = constraint->process(m_processId);

	{
		QDataStream s(&m_serializedProcessData, QIODevice::WriteOnly);
		s.setVersion(QDataStream::Qt_5_3);
		s << process->processName();
		s << *process;
	}

	m_processName = process->processName();
}

void RemoveProcessFromConstraintCommand::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	{
		QDataStream s(&m_serializedProcessData, QIODevice::ReadOnly);
		constraint->createProcess(s);
	}
}

void RemoveProcessFromConstraintCommand::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	constraint->removeProcess(m_processId);
}

int RemoveProcessFromConstraintCommand::id() const
{
	return 1;
}

bool RemoveProcessFromConstraintCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveProcessFromConstraintCommand::serializeImpl(QDataStream& s)
{
	s << m_path << m_processName << m_processId << m_serializedProcessData;
}

void RemoveProcessFromConstraintCommand::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processName >> m_processId >> m_serializedProcessData;
}
