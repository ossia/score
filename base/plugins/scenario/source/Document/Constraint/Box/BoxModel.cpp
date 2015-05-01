#include "BoxModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Deck/DeckModel.hpp"


BoxModel::BoxModel(id_type<BoxModel> id, QObject* parent) :
    IdentifiedObject<BoxModel> {id, "BoxModel", parent}
{

}

BoxModel::BoxModel(BoxModel *source,
                   id_type<BoxModel> id,
                   std::function<void(DeckModel&, DeckModel&)> pvmCopyMethod,
                   QObject *parent) :
    IdentifiedObject<BoxModel> {id, "BoxModel", parent}
{
    for(auto& deck : source->m_decks)
    {
        addDeck(new DeckModel{pvmCopyMethod, deck, deck->id(), this},
                source->deckPosition(deck->id()));
    }
}



ConstraintModel* BoxModel::constraint() const
{
    return static_cast<ConstraintModel*>(this->parent());
}

void BoxModel::addDeck(DeckModel* deck, int position)
{
    // Connection
    connect(this, &BoxModel::on_deleteSharedProcessModel,
            deck, &DeckModel::on_deleteSharedProcessModel);
    m_decks.push_back(deck);
    m_positions.insert(position, deck->id());

    emit deckCreated(deck->id());
    emit deckPositionsChanged();
}

void BoxModel::addDeck(DeckModel* m)
{
    addDeck(m, m_positions.size());
}


void BoxModel::removeDeck(id_type<DeckModel> deckId)
{
    auto removedDeck = deck(deckId);

    // Make the remaining decks decrease their position.
    m_positions.removeAll(deckId);

    // Delete
    vec_erase_remove_if(m_decks,
                        [&deckId](DeckModel * model)
    {
        return model->id() == deckId;
    });

    emit deckRemoved(deckId);
    emit deckPositionsChanged();
    delete removedDeck;
}

void BoxModel::swapDecks(id_type<DeckModel> firstdeck,
                         id_type<DeckModel> seconddeck)
{
    m_positions.swap(m_positions.indexOf(firstdeck), m_positions.indexOf(seconddeck));
    emit deckPositionsChanged();
}

DeckModel* BoxModel::deck(id_type<DeckModel> deckId) const
{
    return findById(m_decks, deckId);
}
