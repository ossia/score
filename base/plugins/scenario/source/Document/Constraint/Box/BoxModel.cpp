#include "BoxModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Deck/DeckModel.hpp"

#include <tools/utilsCPP11.hpp>

#include <QDebug>

BoxModel::BoxModel(id_type<BoxModel> id, QObject* parent):
	IdentifiedObject<BoxModel>{id, "BoxModel", parent}
{

}

void BoxModel::addDeck(DeckModel* deck)
{
	// Reordering : if deck is inserted at position x, what was at x goes at x+1.
	int maxpos = 0;
	for(DeckModel* otherDeck : m_decks)
	{
		maxpos = std::max(maxpos, otherDeck->position());
	}

	if(deck->position() > maxpos)
		deck->setPosition(maxpos + 1);
	else
	{
		for(DeckModel* otherDeck : m_decks)
		{
			if(otherDeck->position() >= deck->position())
			{
				otherDeck->setPosition(otherDeck->position() + 1);
			}
		}
	}


	// Connection
	connect(this, &BoxModel::on_deleteSharedProcessModel,
			deck, &DeckModel::on_deleteSharedProcessModel);
	m_decks.push_back(deck);

	emit deckCreated(deck->id());
}


void BoxModel::removeDeck(id_type<DeckModel> deckId)
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
	vec_erase_remove_if(m_decks,
						[&deckId] (DeckModel* model)
						{ return model->id() == deckId; });

	emit deckRemoved(deckId);
	delete removedDeck;
}

void BoxModel::changeDeckOrder(id_type<DeckModel> deckId, int position)
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}

DeckModel* BoxModel::deck(id_type<DeckModel> deckId) const
{
	return findById(m_decks, deckId);
}
