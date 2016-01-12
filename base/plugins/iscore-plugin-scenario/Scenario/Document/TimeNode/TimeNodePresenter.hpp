#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QPoint>
#include <sys/types.h>
#include <iscore_plugin_scenario_export.h>

class QGraphicsObject;
class QObject;

namespace Scenario
{
class EventModel;
class TimeNodeModel;
class TimeNodeView;
class TriggerPresenter;

class ISCORE_PLUGIN_SCENARIO_EXPORT TimeNodePresenter final :  public NamedObject
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
}
