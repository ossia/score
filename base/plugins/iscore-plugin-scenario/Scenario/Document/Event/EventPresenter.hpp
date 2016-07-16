#pragma once
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/NamedObject.hpp>

#include <iscore/widgets/GraphicsItem.hpp>
#include <QPoint>
#include <QString>
#include <iscore_plugin_scenario_export.h>
class QGraphicsItem;
class QMimeData;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class EventModel;
class EventView;
class ISCORE_PLUGIN_SCENARIO_EXPORT EventPresenter final :
        public QObject
{
        Q_OBJECT

    public:
        EventPresenter(const EventModel& model,
                       QGraphicsItem* parentview,
                       QObject* parent);
        virtual ~EventPresenter();

        const Id<EventModel>& id() const;

        EventView* view() const;
        const EventModel& model() const;

        bool isSelected() const;

        void handleDrop(const QPointF& pos, const QMimeData* mime);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

    private:
        void triggerSetted(QString);
        const EventModel& m_model;
        graphics_item_ptr<EventView> m_view;

        CommandDispatcher<> m_dispatcher;
};
}
