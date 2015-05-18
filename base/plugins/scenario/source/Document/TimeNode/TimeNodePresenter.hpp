#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <QObject>

class TimeNodeView;
class TimeNodeModel;
class EventModel;
class QGraphicsObject;

class TimeNodePresenter :  public NamedObject
{
        Q_OBJECT
    public:
        TimeNodePresenter(const TimeNodeModel& model,
                          QGraphicsObject* parentview,
                          QObject* parent);
        ~TimeNodePresenter();

        const id_type<TimeNodeModel>& id() const;
        int32_t id_val() const
        {
            return *id().val();
        }

        const TimeNodeModel& model() const;
        TimeNodeView* view() const;

        void on_eventAdded(const id_type<EventModel>& eventId);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventAdded(const id_type<EventModel>& eventId,
                        const id_type<TimeNodeModel>& timeNodeId);


    private:
        const TimeNodeModel& m_model;
        TimeNodeView* m_view {};
};
