#include "AddProcessToIntervalCommand.hpp"
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
#include <Document/Interval/IntervalContent/Storey/StoreyModel.hpp>
#include <Document/Interval/IntervalContent/Storey/PositionedStorey/PositionedStoreyModel.hpp>
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
	auto contentModel = interval->contentModel(0);
	auto storey = contentModel->storey(m_createdStoreyId);
	storey->deleteProcessViewModel(m_createdProcessViewModelId);
	contentModel->deleteStorey(storey->id());
	
	interval->deleteProcess(m_createdProcessId);
	m_createdProcessId = -1;
}

void AddProcessToIntervalCommand::redo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	// Create process model
	
	qDebug("YEY");
	m_createdProcessId = interval->createProcess(m_processName);
	qDebug("WOW");
	auto contentModel = interval->contentModel(0);
	// Create storey
	m_createdStoreyId = contentModel->createStorey();
	auto storey = contentModel->storey(m_createdStoreyId);
	
	// Create process view model in the storey
	m_createdProcessViewModelId = storey->createProcessViewModel(m_createdProcessId);
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
