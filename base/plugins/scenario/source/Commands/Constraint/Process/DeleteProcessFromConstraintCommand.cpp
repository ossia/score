#include "DeleteProcessFromConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <QDebug>

using namespace iscore;


DeleteProcessFromConstraintCommand::DeleteProcessFromConstraintCommand(ObjectPath&& constraintPath, int processId):
	SerializableCommand{"ScenarioControl",
						"DeleteProcessCommand",
						"Delete process"},
	m_path{std::move(constraintPath)},
	m_processId{processId}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	auto process = constraint->process(m_processId);

	{
		QDataStream s(&m_serializedProcessData, QIODevice::Append);
		s.setVersion(QDataStream::Qt_5_3);
		s << *process;
	}
	m_processName = process->processName();
}

void DeleteProcessFromConstraintCommand::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	{
		QDataStream s(&m_serializedProcessData, QIODevice::ReadOnly);
		constraint->createProcess(s);
	}
}

void DeleteProcessFromConstraintCommand::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	constraint->deleteProcess(m_processId);
}

int DeleteProcessFromConstraintCommand::id() const
{
	return 1;
}

bool DeleteProcessFromConstraintCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}


// TODO
void DeleteProcessFromConstraintCommand::serializeImpl(QDataStream&)
{
}

void DeleteProcessFromConstraintCommand::deserializeImpl(QDataStream&)
{
}
