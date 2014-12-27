#include "RemoveProcessViewModelFromDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessViewModelFromDeck::RemoveProcessViewModelFromDeck(ObjectPath&& boxPath,
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

		int __warn;
		//TODO
//		DeckModel::saveProcessViewModel(s, pvm);
	}
}

void RemoveProcessViewModelFromDeck::undo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	{
		QDataStream s(&m_serializedProcessViewData, QIODevice::ReadOnly);

		int __warn;
		//TODO
		//deck->createProcessViewModel(s);
	}
}

void RemoveProcessViewModelFromDeck::redo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	deck->deleteProcessViewModel(m_processViewId);
}

int RemoveProcessViewModelFromDeck::id() const
{
	return 1;
}

bool RemoveProcessViewModelFromDeck::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveProcessViewModelFromDeck::serializeImpl(QDataStream& s)
{
	s << m_path << m_processViewId << m_serializedProcessViewData;
}

void RemoveProcessViewModelFromDeck::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processViewId >> m_serializedProcessViewData;
}
