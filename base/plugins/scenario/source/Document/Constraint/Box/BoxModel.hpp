#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class ConstraintModel;
class StoreyModel;

class BoxModel : public IdentifiedObject
{
	Q_OBJECT

	public:
		friend QDataStream& operator << (QDataStream&, const BoxModel&);
		friend QDataStream& operator >> (QDataStream& s, BoxModel& c);
		BoxModel(int id, ConstraintModel* parent);
		BoxModel(QDataStream&, ConstraintModel* parent);

		virtual ~BoxModel() = default;

		int createDeck(int newStoreyId);
		int createDeck(QDataStream& s);
		void removeDeck(int storeyId);
		void changeStoreyOrder(int storeyId, int position);

		StoreyModel* deck(int storeyId) const;

		// Devrait peut-être aller dans une Command à la place ?
		void duplicateStorey();

		const std::vector<StoreyModel*>& decks() const
		{ return m_storeys; }

	signals:
		void deckCreated(int id);
		void deckRemoved(int id);
		void storeyOrderChanged(int storeyId);

		void on_deleteSharedProcessModel(int processId);

		void boxBecomesEmpty(int id);

	private slots:
		void on_storeyBecomesEmpty(int id);

	private:
		int createStorey_impl(StoreyModel* m);
		std::vector<StoreyModel*> m_storeys;
};

