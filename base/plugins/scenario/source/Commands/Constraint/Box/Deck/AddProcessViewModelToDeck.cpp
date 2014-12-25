#include "AddProcessViewModelToDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddProcessViewModelToDeck::AddProcessViewModelToDeck(ObjectPath&& deckPath,
										   int sharedModelId):
	SerializableCommand{"ScenarioControl",
						"AddProcessViewToDeck",
						"Add process view"},
	m_path{deckPath},
	m_sharedModelId{sharedModelId}
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	m_createdProcessViewId = getNextId(deck->processViewModels());
}

void AddProcessViewModelToDeck::undo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	deck->deleteProcessViewModel(m_createdProcessViewId);
}

void AddProcessViewModelToDeck::redo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	deck->createProcessViewModel(m_sharedModelId,
								 m_createdProcessViewId);
}

int AddProcessViewModelToDeck::id() const
{
	return 1;
}

bool AddProcessViewModelToDeck::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddProcessViewModelToDeck::serializeImpl(QDataStream& s)
{
	s << m_path << m_sharedModelId << m_createdProcessViewId;
}

void AddProcessViewModelToDeck::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_sharedModelId >> m_createdProcessViewId;
}
