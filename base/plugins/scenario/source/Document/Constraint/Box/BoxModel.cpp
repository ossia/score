#include "BoxModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Deck/DeckModel.hpp"

#include <tools/utilsCPP11.hpp>

#include <QDebug>

BoxModel::BoxModel(int id, QObject* parent):
	IdentifiedObject{id, "BoxModel", parent}
{

}

int BoxModel::createDeck(int newDeckId)
{
	return addDeck(
				new DeckModel{(int) m_decks.size(),
								newDeckId,
								this});

}

int BoxModel::addDeck(DeckModel* deck)
{
	connect(this, &BoxModel::on_deleteSharedProcessModel,
			deck, &DeckModel::on_deleteSharedProcessModel);
	m_decks.push_back(deck);

	emit deckCreated((SettableIdentifier::identifier_type) deck->id());
	return (SettableIdentifier::identifier_type) deck->id();
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
	vec_erase_remove_if(m_decks,
						[&deckId] (DeckModel* model)
						{ return model->id() == deckId; });

	emit deckRemoved(deckId);
	delete removedDeck;
}

void BoxModel::changeDeckOrder(int deckId, int position)
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}

DeckModel* BoxModel::deck(int deckId) const
{
	return findById(m_decks, deckId);
}
