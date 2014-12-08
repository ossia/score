#pragma once
#include <tools/IdentifiedObject.hpp>
#include <vector>

class IntervalModel;
class PositionedStoreyModel;

// TODO maybe IntervalStoreyGroup instead?
class IntervalContentModel : public IdentifiedObject
{
	Q_OBJECT

	public:
		friend QDataStream& operator << (QDataStream&, const IntervalContentModel&);
		IntervalContentModel(int id, IntervalModel* parent);
		IntervalContentModel(QDataStream&, IntervalModel* parent);

		virtual ~IntervalContentModel() = default;

		int createStorey();
		int createStorey(QDataStream& s);
		void deleteStorey(int storeyId);
		void changeStoreyOrder(int storeyId, int position);

		PositionedStoreyModel* storey(int storeyId);

		// Devrait peut-être aller dans une Command à la place ?
		void duplicateStorey();

		const std::vector<PositionedStoreyModel*>& storeys()
		{ return m_storeys; }

	signals:
		void storeyCreated(int id);
		void storeyDeleted(int id);
		void storeyOrderChanged(int storeyId);

		void on_deleteSharedProcessModel(int processId);

	private:
		int createStorey_impl(PositionedStoreyModel* m);
		std::vector<PositionedStoreyModel*> m_storeys;
};

