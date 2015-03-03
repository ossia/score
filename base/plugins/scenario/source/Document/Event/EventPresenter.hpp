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

        EventView* view() const;
        EventModel* model() const;

        bool isSelected() const;

    signals:
        void pressed();
        void eventMoved(EventData);
        void eventMovedWithControl(EventData);
        void eventReleased();
        void eventReleasedWithControl(EventData);

        void ctrlStateChanged(bool);

        void linesExtremityChange(int, double);

    private:
        EventData pointToEventData(QPointF p) const;

        EventModel* m_model {};
        EventView* m_view {};
};

