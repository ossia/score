#pragma once
#include <tools/IdentifiedObject.hpp>
#include <interface/serialization/VisitorInterface.hpp>

#include <vector>

class ConstraintModel;
class DeckModel;

class BoxModel : public IdentifiedObject
{
	Q_OBJECT

	public:
		BoxModel(int id, QObject* parent);
		template<typename Impl>
		BoxModel(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObject{vis, parent}
		{
			vis.visit(*this);
		}

		virtual ~BoxModel() = default;

		int createDeck(int newDeckId);
		int addDeck(DeckModel* m);

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
		std::vector<DeckModel*> m_decks;
};

