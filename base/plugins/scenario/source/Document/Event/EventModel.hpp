#pragma once
#include <tools/IdentifiedObject.hpp>

namespace OSSIA
{
	class TimeNode;
}
class State;
class IntervalModel;
class EventModel : public IdentifiedObject
{
		Q_OBJECT

		Q_PROPERTY(double heightPercentage
				   READ heightPercentage
				   WRITE setHeightPercentage
				   NOTIFY heightPercentageChanged)

		friend QDataStream& operator << (QDataStream&, const EventModel&);
		friend QDataStream& operator >> (QDataStream&, EventModel&);

	public:
		/// TEMPORARY. This information has to be queried from OSSIA::Scenario instead.
		int m_x{0};

		EventModel(int id, QObject* parent);
		EventModel(int id, double yPos, QObject *parent);
		EventModel(QDataStream& s, QObject* parent);
		virtual ~EventModel() = default;

		const QVector<int>& previousIntervals() const;
		const QVector<int>& nextIntervals() const;

		const std::vector<State*>& states() const;
		void addMessage(QString s);
		void removeMessage(QString s);


		OSSIA::TimeNode* apiObject()
		{ return m_timeNode;}

		double heightPercentage() const;
		int date() const;

	public slots:
		void setHeightPercentage(double arg);

	signals:
		void heightPercentageChanged(double arg);
		void messagesChanged();

	private:
		OSSIA::TimeNode* m_timeNode{};

		QVector<int> m_previousIntervals;
		QVector<int> m_nextIntervals;

		double m_heightPercentage{0.5};

		std::vector<State*> m_states;
};

