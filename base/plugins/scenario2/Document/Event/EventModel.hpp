#pragma once
#include <QNamedObject>

namespace OSSIA
{
	class TimeNode;
}
class IntervalModel;
class EventModel : public QIdentifiedObject
{
	Q_OBJECT
	
	public:
		EventModel(int id, QObject* parent);
		virtual ~EventModel() = default;
		
		const std::vector<IntervalModel*>& previousIntervals() const
		{ return m_previousIntervals; }
		const std::vector<IntervalModel*>& nextIntervals() const
		{ return m_nextIntervals; }
		
		OSSIA::TimeNode* apiObject()
		{ return m_timeNode;}
		
		
	private:
		OSSIA::TimeNode* m_timeNode{};
		
		std::vector<IntervalModel*> m_previousIntervals;
		std::vector<IntervalModel*> m_nextIntervals;
		
};

