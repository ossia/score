#pragma once
#include "Deck/DeckModel.hpp"
#include "source/Document/ModelMetadata.hpp"

#include <ProcessInterface/TimeValue.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class ConstraintModel;
class ProcessModel;

/**
 * @brief The BoxModel class
 *
 * A Box is a deck container.
 * A Box is always found in a Constraint.
 */
class BoxModel : public IdentifiedObject<BoxModel>
{
        Q_OBJECT

    public:
        ModelMetadata metadata; // TODO REMOVEME

        BoxModel(const id_type<BoxModel>& id, QObject* parent);

        // Copy
        BoxModel(const BoxModel& source,
                 const id_type<BoxModel>& id,
                 std::function<void(const DeckModel&, DeckModel&)> pvmCopyMethod,
                 QObject* parent);

        template<typename Impl>
        BoxModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<BoxModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        // A box is necessarily child of a constraint.
        ConstraintModel& constraint() const;

        void addDeck(DeckModel* m, int position);
        void addDeck(DeckModel* m);  // No position : at the end

        void removeDeck(const id_type<DeckModel>& deckId);
        void swapDecks(const id_type<DeckModel>& firstdeck,
                       const id_type<DeckModel>& seconddeck);

        DeckModel* deck(const id_type<DeckModel>& deckId) const;
        int deckPosition(const id_type<DeckModel>& deckId) const
        {
            return m_positions.indexOf(deckId);
        }

        const auto& decks() const
        { return m_decks; }

        const QList<id_type<DeckModel>>& decksPositions() const
        { return m_positions; }

    signals:
        void deckCreated(const id_type<DeckModel>& id);
        void deckRemoved(const id_type<DeckModel>& id);
        void deckPositionsChanged();

        void on_deleteSharedProcessModel(const id_type<ProcessModel>& processId);
        void on_durationChanged(const TimeValue& dur);

    private:
        IdContainer<DeckModel> m_decks;

        // Positions of the decks. First is topmost.
        QList<id_type<DeckModel>> m_positions;
};

