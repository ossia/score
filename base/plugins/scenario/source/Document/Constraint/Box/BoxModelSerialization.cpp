#include <interface/serialization/DataStreamVisitor.hpp>
#include <interface/serialization/JSONVisitor.hpp>
#include "BoxModel.hpp"
#include "Deck/DeckModel.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom (const BoxModel& box)
{
    readFrom (static_cast<const IdentifiedObject<BoxModel>&> (box) );

    m_stream << box.decksPositions();

    auto decks = box.decks();
    m_stream << (int) decks.size();

    for (auto deck : decks)
    {
        readFrom (*deck);
    }

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo (BoxModel& box)
{
    int decks_size;
    QList<id_type<DeckModel>> positions;
    m_stream >> positions;

    m_stream >> decks_size;

    for (; decks_size -- > 0 ;)
    {
        auto deck = new DeckModel (*this, &box);
        box.addDeck (deck, positions.indexOf (deck->id() ) );
    }

    checkDelimiter();
}


template<> void Visitor<Reader<JSON>>::readFrom (const BoxModel& box)
{
    readFrom (static_cast<const IdentifiedObject<BoxModel>&> (box) );

    QJsonArray arr;

    for (auto deck : box.decks() )
    {
        arr.push_back (toJsonObject (*deck) );
    }

    m_obj["Decks"] = arr;

    QJsonArray positions;

    for (auto& id : box.decksPositions() )
    {
        positions.append (*id.val() );
    }

    m_obj["DecksPositions"] = positions;
}

template<> void Visitor<Writer<JSON>>::writeTo (BoxModel& box)
{
    QJsonArray decks = m_obj["Decks"].toArray();
    QJsonArray decksPositions = m_obj["DecksPositions"].toArray();
    QList<id_type<DeckModel>> list;

    for (auto elt : decksPositions)
    {
        list.push_back (id_type<DeckModel> {elt.toInt() });
    }

    for (int i = 0; i < decks.size(); i++)
    {
        Deserializer<JSON> deserializer {decks[i].toObject() };
        auto deck = new DeckModel {deserializer, &box};
        box.addDeck (deck, list.indexOf (deck->id() ) );
    }
}
