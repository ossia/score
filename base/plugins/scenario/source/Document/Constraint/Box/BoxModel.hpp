#pragma once
#include <tools/IdentifiedObject.hpp>
#include <interface/serialization/VisitorInterface.hpp>

#include <vector>

class ConstraintModel;
class DeckModel;
class ProcessSharedModelInterface;
class BoxModel : public IdentifiedObject<BoxModel>
{
	Q_OBJECT

	public:
		BoxModel(id_type<BoxModel> id, QObject* parent);
		template<typename Impl>
		BoxModel(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObject<BoxModel>{vis, parent}
		{
			vis.writeTo(*this);
		}

		virtual ~BoxModel() = default;

		id_type<DeckModel> createDeck(id_type<DeckModel> newDeckId);
		id_type<DeckModel> addDeck(DeckModel* m);

		void removeDeck(id_type<DeckModel> deckId);
		void changeDeckOrder(id_type<DeckModel> deckId, int position);

		DeckModel* deck(id_type<DeckModel> deckId) const;

		const std::vector<DeckModel*>& decks() const
		{ return m_decks; }

	signals:
		void deckCreated(id_type<DeckModel> id);
		void deckRemoved(id_type<DeckModel> id);
		void deckOrderChanged(id_type<DeckModel> deckId);

		void on_deleteSharedProcessModel(id_type<ProcessSharedModelInterface> processId);
		void on_parentDurationChanged(int dur);

	private:
		std::vector<DeckModel*> m_decks;
};

