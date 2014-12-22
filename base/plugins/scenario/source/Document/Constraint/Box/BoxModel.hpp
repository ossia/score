#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class ConstraintModel;
class DeckModel;

class BoxModel : public IdentifiedObject
{
	Q_OBJECT

	public:
		friend QDataStream& operator << (QDataStream&, const BoxModel&);
		friend QDataStream& operator >> (QDataStream& s, BoxModel& c);
		BoxModel(int id, ConstraintModel* parent);
		BoxModel(QDataStream&, ConstraintModel* parent);

		virtual ~BoxModel() = default;

		int createDeck(int newDeckId);
		int createDeck(QDataStream& s);
		void removeDeck(int deckId);
		void changeDeckOrder(int deckId, int position);

		DeckModel* deck(int deckId) const;

		// Devrait peut-être aller dans une Command à la place ?
		void duplicateDeck();

		const std::vector<DeckModel*>& decks() const
		{ return m_decks; }

	signals:
		void deckCreated(int id);
		void deckRemoved(int id);
		void deckOrderChanged(int deckId);

		void on_deleteSharedProcessModel(int processId);

	private:
		int createDeck_impl(DeckModel* m);
		std::vector<DeckModel*> m_decks;
};

