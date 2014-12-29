#include "ResizeDeckVertically.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

ResizeDeckVertically::ResizeDeckVertically():
	SerializableCommand{"ScenarioControl",
						"ResizeDeckVerticallyCommand",
						QObject::tr("Resize Deck")}
{
}

ResizeDeckVertically::ResizeDeckVertically(ObjectPath&& deckPath,
										   int newSize):
	SerializableCommand{"ScenarioControl",
						"ResizeDeckVerticallyCommand",
						QObject::tr("Resize Deck")},
	m_path{deckPath},
	m_newSize{newSize}
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	m_originalSize = deck->height();
}

void ResizeDeckVertically::undo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	deck->setHeight(m_originalSize);
}

void ResizeDeckVertically::redo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	deck->setHeight(m_newSize);
}

int ResizeDeckVertically::id() const
{
	return 1;
}

bool ResizeDeckVertically::mergeWith(const QUndoCommand* other)
{
	return false;
}

void ResizeDeckVertically::serializeImpl(QDataStream& s)
{
	s << m_path << m_originalSize << m_newSize;
}

// Would be better in a ctor ?
void ResizeDeckVertically::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_originalSize >> m_newSize;
}
