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
        void updateStateView() const;

        void handleDrop(const QMimeData* mime);

        void on_previousConstraintsChanged();
        void on_nextConstraintsChanged();

        void updateMinExtremities(const id_type<ConstraintModel>&, const double);
        void updateMaxExtremities(const id_type<ConstraintModel> &, const double);
        const QPair<id_type<ConstraintModel>, double> extremityMin() const;
        const QPair<id_type<ConstraintModel>, double> extremityMax() const;

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

        void heightPercentageChanged();

    private slots:
        void triggerSetted(QString);

    private:
        void constraintsChangedHelper(
                const QVector<id_type<ConstraintModel>>& ids,
                QVector<QMetaObject::Connection>& connections);

        QPair<id_type<ConstraintModel>, double> m_extremityMin;
        QPair<id_type<ConstraintModel>, double> m_extremityMax;

        const EventModel& m_model;
        EventView* m_view {};

        CommandDispatcher<> m_dispatcher;
        QVector<QMetaObject::Connection> m_previousConstraintsConnections;
        QVector<QMetaObject::Connection> m_nextConstraintsConnections;
};

