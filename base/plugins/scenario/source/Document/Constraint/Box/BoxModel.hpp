#pragma once
#include <tools/SettableIdentifierAlternative.hpp>
#include <interface/serialization/VisitorInterface.hpp>

#include <vector>

class ConstraintModel;
class DeckModel;

class BoxModel : public IdentifiedObjectAlternative<BoxModel>
{
	Q_OBJECT

	public:
		BoxModel(id_type<BoxModel> id, QObject* parent);
		template<typename Impl>
		BoxModel(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObjectAlternative<BoxModel>{vis, parent}
		{
			vis.writeTo(*this);
		}

		virtual ~BoxModel() = default;

		int createDeck(int newDeckId);
		int addDeck(DeckModel* m);

		void removeDeck(int deckId);
		void changeDeckOrder(int deckId, int position);

		DeckModel* deck(int deckId) const;

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

