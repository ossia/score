#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifierAlternative.hpp>
#include "Document/Event/EventData.hpp"

class EventModel;
using IdentifiedEventModel = id_mixin<EventModel>;
class EventView;

class EventPresenter : public NamedObject
{
	Q_OBJECT

	public:
		EventPresenter(IdentifiedEventModel* model, EventView* view, QObject* parent);
		virtual ~EventPresenter();

		id_type<EventModel> id() const;
		EventView* view() const;
		IdentifiedEventModel* model() const;

		bool isSelected() const;
		void deselect();

	signals:
		void eventSelected(id_type<EventModel>);
		void eventReleasedWithControl(EventData);
		void eventReleased(EventData);
		void linesExtremityChange(double, double);


		void elementSelected(QObject*);

	public slots:
		void on_eventReleased(QPointF);
		void on_eventReleasedWithControl(QPointF, QPointF);

	private:
		IdentifiedEventModel* m_model{};
		EventView* m_view{};
};

