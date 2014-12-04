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

		Q_PROPERTY(double heightPercentage READ heightPercentage WRITE setHeightPercentage NOTIFY heightPercentageChanged)
	public:
		/// TEMPORARY. This information has to be queried from OSSIA::Scenario instead.
		int m_x{0};
		friend QDataStream& operator << (QDataStream&, const EventModel&);

		EventModel(int id, QObject* parent);
		EventModel(int id, double yPos, QObject *parent);
		EventModel(QDataStream& s, QObject* parent);
		virtual ~EventModel() = default;

		const QVector<int>& previousIntervals() const;
		const QVector<int>& nextIntervals() const;

		OSSIA::TimeNode* apiObject()
		{ return m_timeNode;}

		double heightPercentage() const;

	public slots:
		void setHeightPercentage(double arg);

	signals:
		void heightPercentageChanged(double arg);

	private:
		OSSIA::TimeNode* m_timeNode{};

		QVector<int> m_previousIntervals;
		QVector<int> m_nextIntervals;

		double m_heightPercentage{0.5};
};

