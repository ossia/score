#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>

#include <QObject>

class TimeNodeView;
class TimeNodeModel;
class EventModel;
struct EventData;

class TimeNodePresenter :  public NamedObject
{
		Q_OBJECT
	public:
		explicit TimeNodePresenter(TimeNodeModel* model, TimeNodeView* view, QObject *parent);
		~TimeNodePresenter();

		id_type<TimeNodeModel> id() const;
		int32_t id_val() const
		{ return *id().val(); }

		TimeNodeModel* model();
		TimeNodeView* view();

		bool isSelected();
		void deselect();

	signals:
		void timeNodeMoved(EventData);
		void timeNodeReleased();
		void elementSelected(QObject*);
        void eventAdded(id_type<EventModel> eventId, id_type<TimeNodeModel> timeNodeId);
        void eventSelected(QString);

	public slots:
		void on_timeNodeMoved(QPointF);
        void on_eventAdded(id_type<EventModel> eventId);

	private:
		TimeNodeModel* m_model{};
		TimeNodeView* m_view{};
};
