#pragma once
#include <tools/IdentifiedObject.hpp>
#include <interface/serialization/VisitorInterface.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <vector>

class ConstraintModel;
class DeckModel;
class ProcessSharedModelInterface;

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
        BoxModel (id_type<BoxModel> id, QObject* parent);

        // Copy
        BoxModel (BoxModel* source, id_type<BoxModel> id, QObject* parent);

        template<typename Impl>
        BoxModel (Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<BoxModel> {vis, parent}
        {
            vis.writeTo (*this);
        }

        virtual ~BoxModel() = default;

        ConstraintModel* constraint() const;

        void addDeck (DeckModel* m, int position);
        void addDeck (DeckModel* m); // No position : at the end

        void removeDeck (id_type<DeckModel> deckId);
        void changeDeckOrder (id_type<DeckModel> deckId, int position);

        DeckModel* deck (id_type<DeckModel> deckId) const;
        int deckPosition (id_type<DeckModel> deckId) const
        {
            return m_positions.indexOf (deckId);
        }

        const std::vector<DeckModel*>& decks() const
        {
            return m_decks;
        }
        const QList<id_type<DeckModel>>& decksPositions() const
        {
            return m_positions;
        }

    signals:
        void deckCreated (id_type<DeckModel> id);
        void deckRemoved (id_type<DeckModel> id);
        void deckPositionsChanged();

        void on_deleteSharedProcessModel (id_type<ProcessSharedModelInterface> processId);
        void on_durationChanged (TimeValue dur);

    private:
        std::vector<DeckModel*> m_decks;

        // Positions of the decks. First is topmost.
        QList<id_type<DeckModel>> m_positions;
};

