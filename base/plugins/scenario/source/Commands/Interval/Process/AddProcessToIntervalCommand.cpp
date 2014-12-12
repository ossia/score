#include "AddProcessToIntervalCommand.hpp"

#include "Document/Interval/IntervalModel.hpp"
#include "Document/Interval/IntervalContent/IntervalContentModel.hpp"
#include "Document/Interval/IntervalContent/Storey/StoreyModel.hpp"
#include "Document/Interval/IntervalContent/Storey/PositionedStorey/PositionedStoreyModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <QDebug>
using namespace iscore;

// TODO switch between content models.
AddProcessToIntervalCommand::AddProcessToIntervalCommand(ObjectPath&& intervalPath, QString process):
	SerializableCommand{"ScenarioControl",
						"AddProcessToIntervalCommand",
						"Add process"},
	m_path(std::move(intervalPath)),
	m_processName{process}
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	m_createdProcessId = getNextId(interval->processes());
	m_contentModelId = interval->contentModels().front()->id(); // TODO pass as arg of the command.
	m_createdStoreyId = getNextId(interval->contentModel(m_contentModelId)->storeys());
	m_createdProcessViewModelId = getNextId(); // Storey does not exist yet so we can safely do this.
}

void AddProcessToIntervalCommand::undo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());
	auto contentModel = interval->contentModel(m_contentModelId);
	auto storey = contentModel->storey(m_createdStoreyId);
	storey->deleteProcessViewModel(m_createdProcessViewModelId);
	contentModel->deleteStorey(storey->id());

	interval->deleteProcess(m_createdProcessId);
}

void AddProcessToIntervalCommand::redo()
{
	auto interval = static_cast<IntervalModel*>(m_path.find());

	// Create process model
	interval->createProcess(m_processName, m_createdProcessId);
	auto contentModel = interval->contentModel(m_contentModelId);

	// Create storey
	contentModel->createStorey(m_createdStoreyId);
	auto storey = contentModel->storey(m_createdStoreyId);

	// Create process view model in the storey
	storey->createProcessViewModel(m_createdProcessId, m_createdProcessViewModelId);
}

int AddProcessToIntervalCommand::id() const
{
	return 1;
}

bool AddProcessToIntervalCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddProcessToIntervalCommand::serializeImpl(QDataStream&)
{
}

void AddProcessToIntervalCommand::deserializeImpl(QDataStream&)
{
}
