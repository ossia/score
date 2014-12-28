#include <interface/serialization/DataStreamVisitor.hpp>
#include <interface/serialization/JSONVisitor.hpp>
#include "BoxModel.hpp"
#include "Deck/DeckModel.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom(const BoxModel& box)
{
	readFrom(static_cast<const IdentifiedObject&>(box));

	auto decks = box.decks();
	m_stream << (int)decks.size();
	for(auto deck : decks)
	{
		readFrom(*deck);
	}
}

template<> void Visitor<Writer<DataStream>>::writeTo(BoxModel& box)
{
	int decks_size;
	m_stream >> decks_size;

	for(; decks_size --> 0 ;)
	{
		auto deck = new DeckModel(*this, &box);
		box.addDeck(deck);
	}
}


template<> void Visitor<Reader<JSON>>::readFrom(const BoxModel& box)
{
	readFrom(static_cast<const IdentifiedObject&>(box));

	QJsonArray arr;
	for(auto deck : box.decks())
	{
		arr.push_back(toJsonObject(*deck));
	}

	m_obj["Decks"] = arr;
}

template<> void Visitor<Writer<JSON>>::writeTo(BoxModel& box)
{
	QJsonArray arr = m_obj["Decks"].toArray();

	for(auto json_vref : arr)
	{
		Deserializer<JSON> deserializer{json_vref.toObject()};
		auto deck = new DeckModel{deserializer, &box};
		box.addDeck(deck);
	}
}