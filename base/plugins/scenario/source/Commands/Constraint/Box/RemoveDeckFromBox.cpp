#include "RemoveDeckFromBox.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "interface/serialization/DataStreamVisitor.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveDeckFromBox::RemoveDeckFromBox():
	RemoveDeckFromBox{{}, {}}
{
}

RemoveDeckFromBox::RemoveDeckFromBox(ObjectPath&& boxPath, int deckId):
	SerializableCommand{"ScenarioControl",
						"RemoveDeckFromBox",
						"Remove deck"},
	m_path{boxPath},
	m_deckId{deckId}
{
	auto box = static_cast<BoxModel*>(m_path.find());
	Serializer<DataStream> s{&m_serializedDeckData};

	s.readFrom(*box->deck(deckId));
}

void RemoveDeckFromBox::undo()
{
	auto box = static_cast<BoxModel*>(m_path.find());
	Deserializer<DataStream> s{&m_serializedDeckData};
	box->addDeck(new DeckModel{s, box});
}

void RemoveDeckFromBox::redo()
{
	auto box = static_cast<BoxModel*>(m_path.find());
	box->removeDeck(m_deckId);
}

int RemoveDeckFromBox::id() const
{
	return 1;
}

bool RemoveDeckFromBox::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveDeckFromBox::serializeImpl(QDataStream& s)
{
	s << m_path << m_deckId << m_serializedDeckData;
}

void RemoveDeckFromBox::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_deckId >> m_serializedDeckData;
}
