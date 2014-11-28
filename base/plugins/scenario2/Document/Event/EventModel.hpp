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
		friend QDataStream& operator << (QDataStream&, const EventModel&);
		
		EventModel(int id, QObject* parent);
		EventModel(QDataStream& s, QObject* parent);
		virtual ~EventModel() = default;
		
		const QVector<int>& previousIntervals() const
		{ return m_previousIntervals; }
		const QVector<int>& nextIntervals() const
		{ return m_nextIntervals; }
		
		OSSIA::TimeNode* apiObject()
		{ return m_timeNode;}
		
		
	private:
		OSSIA::TimeNode* m_timeNode{};
		
		QVector<int> m_previousIntervals;
		QVector<int> m_nextIntervals;
		
};

