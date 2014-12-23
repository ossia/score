#include "BoxModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Deck/DeckModel.hpp"

#include <tools/utilsCPP11.hpp>

#include <QDebug>

QDataStream& operator << (QDataStream& s, const BoxModel& c)
{
	s << static_cast<const IdentifiedObject&>(c);
	s << (int)c.m_decks.size();
	for(auto& deck : c.m_decks)
	{
		s << *deck;
	}

	return s;
}

QDataStream& operator >> (QDataStream& s, BoxModel& c)
{
	int decks_size;
	s >> decks_size;

	for(; decks_size --> 0 ;)
	{
		c.createDeck(s);
	}

	return s;
}


BoxModel::BoxModel(int id, QObject* parent):
	IdentifiedObject{id, "BoxModel", parent}
{

}

BoxModel::BoxModel(QDataStream& s, QObject* parent):
	IdentifiedObject{s, parent}
{
	s >> *this;
}

int BoxModel::createDeck(int newDeckId)
{
	return createDeck_impl(
				new DeckModel{(int) m_decks.size(),
								newDeckId,
								this});

}

int BoxModel::createDeck(QDataStream& s)
{
	return createDeck_impl(
				new DeckModel{s,
								this});
}

int BoxModel::createDeck_impl(DeckModel* deck)
{
	connect(this,	&BoxModel::on_deleteSharedProcessModel,
			deck, &DeckModel::on_deleteSharedProcessModel);
	m_decks.push_back(deck);

	emit deckCreated(deck->id());
	return deck->id();
}


void BoxModel::removeDeck(int deckId)
{
	auto removedDeck = deck(deckId);

	// Make the remaining decks decrease their position.
	for(DeckModel* deck : m_decks)
	{
		auto pos = deck->position();
		if(pos > removedDeck->position())
			deck->setPosition(pos - 1);
	}

	// Delete
	removeById(m_decks, deckId);

	emit deckRemoved(deckId);
}

void BoxModel::changeDeckOrder(int deckId, int position)
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}

DeckModel* BoxModel::deck(int deckId) const
{
	return findById(m_decks, deckId);
}

void BoxModel::duplicateDeck()
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}
