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
        EventPresenter (EventModel* model, EventView* view, QObject* parent);
        virtual ~EventPresenter();

        id_type<EventModel> id() const;
        int32_t id_val() const
        {
            return *id().val();
        }

        EventView* view() const;
        EventModel* model() const;

        bool isSelected() const;
        void deselect();

    signals:
        void eventSelected (id_type<EventModel>);

        void eventMoved (EventData);
        void eventMovedWithControl (EventData);
        void eventReleased();
        void eventReleasedWithControl (EventData);

        void ctrlStateChanged (bool);

        void linesExtremityChange (int, double);

        void elementSelected (QObject*);
        void constraintSelected (QString);
        void inspectPreviousElement();
        void eventInspectorCreated (id_type<EventModel>);

    private:
        EventData pointToEventData (QPointF p) const;

        EventModel* m_model {};
        EventView* m_view {};
};

