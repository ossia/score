#include "AddProcessViewToDeck.hpp"

#include "Document/Constraint/Box/Storey/StoreyModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddProcessViewToDeck::AddProcessViewToDeck(ObjectPath&& deckPath,
										   int sharedModelId):
	SerializableCommand{"ScenarioControl",
						"AddProcessViewToDeck",
						"Add process view"},
	m_path{deckPath},
	m_sharedModelId{sharedModelId}
{
	auto deck = static_cast<StoreyModel*>(m_path.find());
	m_createdProcessViewId = getNextId(deck->processViewModels());
}

void AddProcessViewToDeck::undo()
{
	auto deck = static_cast<StoreyModel*>(m_path.find());
	deck->deleteProcessViewModel(m_createdProcessViewId);
}

void AddProcessViewToDeck::redo()
{
	auto deck = static_cast<StoreyModel*>(m_path.find());
	deck->createProcessViewModel(m_sharedModelId,
								 m_createdProcessViewId);
}

int AddProcessViewToDeck::id() const
{
	return 1;
}

bool AddProcessViewToDeck::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddProcessViewToDeck::serializeImpl(QDataStream& s)
{
	s << m_path << m_sharedModelId << m_createdProcessViewId;
}

void AddProcessViewToDeck::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_sharedModelId >> m_createdProcessViewId;
}
