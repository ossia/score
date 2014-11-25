#include "DeleteProcessFromIntervalCommand.hpp"
#include <Document/Interval/IntervalModel.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>

using namespace iscore;


DeleteProcessFromIntervalCommand::DeleteProcessFromIntervalCommand(ObjectPath&& intervalPath, QString processName, int processId):
	SerializableCommand{"ScenarioControl",
						"DeleteProcessCommand",
						"Delete process"},
	m_path(std::move(intervalPath)),
	m_processName{processName},
	m_processId{processId}
{
}

void DeleteProcessFromIntervalCommand::undo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	interval->createProcess(m_processName, m_serializedProcessData);
}

void DeleteProcessFromIntervalCommand::redo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	auto process = interval->process(m_processId);
	
	m_serializedProcessData = process->serialize();
	interval->deleteProcess(m_processId);
}

int DeleteProcessFromIntervalCommand::id() const
{
	return 1;
}

bool DeleteProcessFromIntervalCommand::mergeWith(const QUndoCommand* other)
{
}

void DeleteProcessFromIntervalCommand::serializeImpl(QDataStream&)
{
}

void DeleteProcessFromIntervalCommand::deserializeImpl(QDataStream&)
{
}
