#include "AddProcessToIntervalCommand.hpp"
#include <Document/Interval/IntervalModel.hpp>

using namespace iscore;


AddProcessToIntervalCommand::AddProcessToIntervalCommand(ObjectPath&& intervalPath, QString process):
	SerializableCommand{"ScenarioControl",
						"AddProcessToIntervalCommand",
						"Add process"},
	m_path(std::move(intervalPath)),
	m_processName{process}
{
	
}

void AddProcessToIntervalCommand::undo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	interval->deleteProcess(m_createdProcessId);
	m_createdProcessId = -1;
}

void AddProcessToIntervalCommand::redo()
{
	qDebug(Q_FUNC_INFO);

	auto interval = static_cast<IntervalModel*>(m_path.find());
	m_createdProcessId = interval->createProcess(m_processName);
}

int AddProcessToIntervalCommand::id() const
{
	return 1;
}

bool AddProcessToIntervalCommand::mergeWith(const QUndoCommand* other)
{
}

void AddProcessToIntervalCommand::serializeImpl(QDataStream&)
{
}

void AddProcessToIntervalCommand::deserializeImpl(QDataStream&)
{
}
