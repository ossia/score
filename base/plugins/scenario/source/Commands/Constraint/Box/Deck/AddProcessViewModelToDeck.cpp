#include "AddProcessViewModelToDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddProcessViewModelToDeck::AddProcessViewModelToDeck():
	SerializableCommand{"ScenarioControl",
						"AddProcessViewModelToDeck",
						QObject::tr("Add process view")}
{
}

AddProcessViewModelToDeck::AddProcessViewModelToDeck(
										ObjectPath&& deckPath,
										int sharedModelId):
	SerializableCommand{"ScenarioControl",
						"AddProcessViewModelToDeck",
						QObject::tr("Add process view")},
	m_path{deckPath},
	m_sharedModelId{sharedModelId}
{
	auto deck = m_path.find<DeckModel>();
	m_createdProcessViewId = getStrongId(deck->processViewModels());
}

void AddProcessViewModelToDeck::undo()
{
	auto deck = m_path.find<DeckModel>();
	deck->deleteProcessViewModel(m_createdProcessViewId);
}

void AddProcessViewModelToDeck::redo()
{
	auto deck = m_path.find<DeckModel>();
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
