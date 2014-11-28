#include "DeleteProcessFromIntervalCommand.hpp"
#include <Document/Interval/IntervalModel.hpp>
#include <interface/process/ProcessSharedModelInterface.hpp>
#include <QDebug>
using namespace iscore;


DeleteProcessFromIntervalCommand::DeleteProcessFromIntervalCommand(ObjectPath&& intervalPath, QString processName, int processId):
	SerializableCommand{"ScenarioControl",
						"DeleteProcessCommand",
						"Delete process"},
	m_path(std::move(intervalPath)),
	m_processId{processId}
{
}

void DeleteProcessFromIntervalCommand::undo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	{
		QDataStream s(&m_serializedProcessData, QIODevice::ReadOnly);
		qDebug() << m_processName;
		interval->createProcess(m_processName, s);
	}
}

void DeleteProcessFromIntervalCommand::redo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	auto process = interval->process(m_processId);

	{
		QDataStream s(&m_serializedProcessData, QIODevice::Append);
		s.setVersion(QDataStream::Qt_5_3);
		s << *process;
	}
	m_processName = process->processName();

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
