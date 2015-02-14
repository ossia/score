#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>
#include "Document/Event/EventData.hpp"

class EventModel;
class EventView;

class EventPresenter : public NamedObject
{
	Q_OBJECT

	public:
		EventPresenter(EventModel* model, EventView* view, QObject* parent);
		virtual ~EventPresenter();

		id_type<EventModel> id() const;
		int32_t id_val() const
		{ return *id().val(); }

		EventView* view() const;
		EventModel* model() const;

		bool isSelected() const;
		void deselect();

	signals:
		void eventSelected(id_type<EventModel>);
		void eventReleasedWithControl(EventData);
		void eventMoved(EventData);
		void eventReleased();
        void linesExtremityChange(int, double);

		void elementSelected(QObject*);

	public slots:
		void on_eventMoved(QPointF);
		void on_eventReleasedWithControl(QPointF, QPointF);

	private:
		EventData pointToEventData(QPointF p) const;

		EventModel* m_model{};
		EventView* m_view{};
};

