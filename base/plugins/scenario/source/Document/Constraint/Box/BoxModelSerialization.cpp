#include <interface/serialization/DataStreamVisitor.hpp>
#include "BoxModel.hpp"
#include "Deck/DeckModel.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom(const BoxModel& box)
{
	readFrom(static_cast<const IdentifiedObject&>(box));

	auto decks = box.decks();
	m_stream << (int)decks.size();
	for(const DeckModel* deck : decks)
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
		DeckModel* deck = new DeckModel(*this, &box);
		box.addDeck(deck);
	}
}