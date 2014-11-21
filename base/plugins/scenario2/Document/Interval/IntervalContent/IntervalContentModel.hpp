#pragma once
#include <QNamedObject>
#include <vector>

class IntervalModel;
class PositionedStoreyModel;

class IntervalContentModel : public QNamedObject
{
	Q_OBJECT
	
	public:
		IntervalContentModel(IntervalModel* parent);
		
		virtual ~IntervalContentModel() = default;
		int id() const 
		{ return m_id; }
		
		void createStorey();
		void deleteStorey(int storeyId);
		void changeStoreyOrder(int storeyId, int position);
		void duplicateStorey();
		
	signals:
		void storeyCreated(int id);
		void storeyDeleted(int id);
		void storeyOrderChanged(int storeyId);
		
	private:
		std::vector<PositionedStoreyModel*> m_storeys;
		
		int m_id{};
		
		int m_nextStoreyId{};
};

