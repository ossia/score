#pragma once
#include <tools/IdentifiedObject.hpp>
#include <tools/SettableIdentifier.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>

class EventModel;
class TimeNodeModel : public IdentifiedObject<TimeNodeModel>
{
	Q_OBJECT

	public:
		TimeNodeModel(id_type<TimeNodeModel> id, QObject* parent);
		TimeNodeModel(id_type<TimeNodeModel> id, int date, QObject *parent);
		~TimeNodeModel();

		void addEvent(id_type<EventModel>);
		bool removeEvent(id_type<EventModel>);

		double top() const;
		double bottom() const;
		int date() const;

        void setDate(int);

        bool isEmpty();

		double y() const;
		void setY(double y);

	signals:

	public slots:

	private:
		int m_date{0};
		double m_y{0.0};

		QVector<id_type<EventModel>> m_events;
//	        QMap<int, double> m_yPoints;
	
		// @todo : maybe not useful ...
		double m_topY{0.0};
		double m_bottomY{0.0};
};
