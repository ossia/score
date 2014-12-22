#include "RemoveProcessViewFromDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessViewFromDeck::RemoveProcessViewFromDeck(ObjectPath&& boxPath,
													 int processViewId):
	SerializableCommand{"ScenarioControl",
						"RemoveProcessViewFromDeck",
						"Remove process view"},
	m_path{boxPath},
	m_processViewId{processViewId}
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	auto pvm = deck->processViewModel(m_processViewId);
	{
		QDataStream s(&m_serializedProcessViewData, QIODevice::WriteOnly);
		s.setVersion(QDataStream::Qt_5_3);

		s << *pvm;
	}

	m_sharedModelId = pvm->sharedProcessId();
}

void RemoveProcessViewFromDeck::undo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	{
		QDataStream s(&m_serializedProcessViewData, QIODevice::ReadOnly);
		deck->createProcessViewModel(s, m_sharedModelId);
	}
}

void RemoveProcessViewFromDeck::redo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	deck->deleteProcessViewModel(m_processViewId);
}

int RemoveProcessViewFromDeck::id() const
{
	return 1;
}

bool RemoveProcessViewFromDeck::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveProcessViewFromDeck::serializeImpl(QDataStream& s)
{
	s << m_path << m_processViewId << m_serializedProcessViewData;
}

void RemoveProcessViewFromDeck::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processViewId >> m_serializedProcessViewData;
}
