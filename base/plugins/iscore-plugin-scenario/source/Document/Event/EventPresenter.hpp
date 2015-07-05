#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include "EventHalves.hpp"
class QGraphicsObject;
class EventModel;
class EventView;
class TemporalScenarioPresenter;
class QMimeData;
class ConstraintModel;
class EventPresenter : public NamedObject
{
        Q_OBJECT

    public:
        EventPresenter(const EventModel& model,
                       QGraphicsObject* parentview,
                       QObject* parent);
        virtual ~EventPresenter();

        const id_type<EventModel>& id() const;

        EventView* view() const;
        const EventModel& model() const;

        bool isSelected() const;

        void handleDrop(const QMimeData* mime);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

    private slots:
        void triggerSetted(QString);

    private:
        const EventModel& m_model;
        EventView* m_view {};

        CommandDispatcher<> m_dispatcher;
};

