#include "BoxModelSerialization.hpp"
#include "BoxModel.hpp"
#include "Deck/DeckModel.hpp"

template<> void Visitor<Reader<DataStream>>::visit(const BoxModel& box)
{
	visit(static_cast<const IdentifiedObject&>(box));

	auto decks = box.decks();
	m_stream << (int)decks.size();
	for(const DeckModel* deck : decks)
	{
		visit(*deck);
	}
}

template<> void Visitor<Writer<DataStream>>::visit(BoxModel& box)
{
	int decks_size;
	m_stream >> decks_size;

	for(; decks_size --> 0 ;)
	{
		DeckModel* deck = new DeckModel(*this, &box);
		box.addDeck(deck);
	}
}