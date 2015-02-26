#include "RemoveDeckFromBox.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "interface/serialization/DataStreamVisitor.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveDeckFromBox::RemoveDeckFromBox() :
    SerializableCommand {"ScenarioControl",
    "RemoveDeckFromBox",
    QObject::tr ("Remove deck")
}
{
}

RemoveDeckFromBox::RemoveDeckFromBox (ObjectPath&& deckPath) :
    SerializableCommand {"ScenarioControl",
    "RemoveProcessViewModelFromDeck",
    QObject::tr ("Remove process view")
}
{
    auto boxPath = deckPath.vec();
    auto lastId = boxPath.takeLast();
    m_path = ObjectPath{std::move (boxPath) };
    m_deckId = id_type<DeckModel> (lastId.id() );

    auto box = m_path.find<BoxModel>();
    m_position = box->deckPosition (m_deckId);
}

RemoveDeckFromBox::RemoveDeckFromBox (ObjectPath&& boxPath, id_type<DeckModel> deckId) :
    SerializableCommand {"ScenarioControl",
    "RemoveDeckFromBox",
    QObject::tr ("Remove deck")
},
m_path {boxPath},
m_deckId {deckId}
{
    auto box = m_path.find<BoxModel>();
    Serializer<DataStream> s{&m_serializedDeckData};

    s.readFrom (*box->deck (deckId) );
    m_position = box->deckPosition (deckId);
}

void RemoveDeckFromBox::undo()
{
    auto box = m_path.find<BoxModel>();
    Deserializer<DataStream> s {&m_serializedDeckData};
    box->addDeck (new DeckModel {s, box}, m_position);
}

void RemoveDeckFromBox::redo()
{
    auto box = m_path.find<BoxModel>();
    box->removeDeck (m_deckId);
}

int RemoveDeckFromBox::id() const
{
    return 1;
}

bool RemoveDeckFromBox::mergeWith (const QUndoCommand* other)
{
    return false;
}

void RemoveDeckFromBox::serializeImpl (QDataStream& s) const
{
    s << m_path << m_deckId << m_serializedDeckData << m_position;
}

void RemoveDeckFromBox::deserializeImpl (QDataStream& s)
{
    s >> m_path >> m_deckId >> m_serializedDeckData >> m_position;
}
