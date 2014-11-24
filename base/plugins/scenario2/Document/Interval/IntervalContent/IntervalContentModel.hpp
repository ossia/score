#pragma once
#include <QNamedObject>
#include <vector>

class IntervalModel;
class PositionedStoreyModel;

class IntervalContentModel : public QIdentifiedObject
{
	Q_OBJECT
	
	public:
		IntervalContentModel(int id, IntervalModel* parent);
		
		virtual ~IntervalContentModel() = default;
		
		void createStorey();
		void deleteStorey(int storeyId);
		void changeStoreyOrder(int storeyId, int position);
		
		PositionedStoreyModel* storey(int storeyId);
		
		// Devrait peut-être aller dans une Command à la place ?
		void duplicateStorey();
		
	signals:
		void storeyCreated(int id);
		void storeyDeleted(int id);
		void storeyOrderChanged(int storeyId);
		
	private:
		std::vector<PositionedStoreyModel*> m_storeys;
		
		int m_nextStoreyId{};
};

