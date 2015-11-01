#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <QObject>
#include <QMap>

class TimeNodeView;
class TimeNodeModel;
class EventModel;
class QGraphicsObject;
class TriggerPresenter;

class TimeNodePresenter :  public NamedObject
{
        Q_OBJECT
    public:
        TimeNodePresenter(const TimeNodeModel& model,
                          QGraphicsObject* parentview,
                          QObject* parent);
        ~TimeNodePresenter();

        const Id<TimeNodeModel>& id() const;
        int32_t id_val() const
        {
            return *id().val();
        }

        const TimeNodeModel& model() const;
        TimeNodeView* view() const;

        void on_eventAdded(const Id<EventModel>& eventId);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventAdded(const Id<EventModel>& eventId,
                        const Id<TimeNodeModel>& timeNodeId);

    private:
        const TimeNodeModel& m_model;
        TimeNodeView* m_view {};
        TriggerPresenter* m_triggerPres;
 };
