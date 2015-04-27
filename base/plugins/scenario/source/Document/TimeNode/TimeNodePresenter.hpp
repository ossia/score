#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <QObject>

class TimeNodeView;
class TimeNodeModel;
class EventModel;

class TimeNodePresenter :  public NamedObject
{
        Q_OBJECT
    public:
        explicit TimeNodePresenter(TimeNodeModel* model,
                                   QGraphicsObject* parentview,
                                   QObject* parent);
        ~TimeNodePresenter();

        id_type<TimeNodeModel> id() const;
        int32_t id_val() const
        {
            return *id().val();
        }

        TimeNodeModel* model() const;
        TimeNodeView* view() const;

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventAdded(id_type<EventModel> eventId, id_type<TimeNodeModel> timeNodeId);

    public slots:
        void on_eventAdded(id_type<EventModel> eventId);

    private:
        TimeNodeModel* m_model {};
        TimeNodeView* m_view {};
};
