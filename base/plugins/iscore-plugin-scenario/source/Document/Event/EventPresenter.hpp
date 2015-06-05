#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
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
        void updateViewHalves() const;

        void handleDrop(const QMimeData* mime);

        void on_previousConstraintsChanged();
        void on_nextConstraintsChanged();

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

        void heightPercentageChanged();


    private:
        void constraintsChangedHelper(
                const QVector<id_type<ConstraintModel>>& ids,
                QVector<QMetaObject::Connection>& connections);

        Scenario::EventHalves m_halves{Scenario::EventHalves::None};

        const EventModel& m_model;
        EventView* m_view {};

        CommandDispatcher<> m_dispatcher;
        QVector<QMetaObject::Connection> m_previousConstraintsConnections;
        QVector<QMetaObject::Connection> m_nextConstraintsConnections;
};

