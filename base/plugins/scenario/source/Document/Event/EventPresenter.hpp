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

		bool isSelected() const;
		void deselect();

	signals:
		void eventSelected(int);
		void eventReleasedWithControl(EventData);
		void eventReleased(EventData);
		void linesExtremityChange(double, double);


		void elementSelected(QObject*);

	public slots:
		void on_eventReleased(QPointF);
        void on_eventReleasedWithControl(QPointF, QPointF);

	private:
		EventModel* m_model{};
		EventView* m_view{};
};

