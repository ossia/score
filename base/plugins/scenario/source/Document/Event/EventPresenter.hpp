#pragma once
#include <tools/NamedObject.hpp>
#include "Document/Event/EventData.hpp"


class EventModel;
class EventView;

class EventPresenter : public NamedObject
{
	Q_OBJECT

	public:
		EventPresenter(EventModel* model, EventView* view, QObject* parent);
		virtual ~EventPresenter();

		int id() const;
        EventView* view() const;
        EventModel* model() const;

	signals:
		void eventSelected(int id);
        void eventReleasedWithControl(EventData d);
        void eventReleased(EventData d);

		void elementSelected(QObject*);

    public slots:
        void on_eventReleased(QPointF);
        void on_eventReleasedWithControl(QPointF);

	private:
		EventModel* m_model{};
		EventView* m_view{};
};

