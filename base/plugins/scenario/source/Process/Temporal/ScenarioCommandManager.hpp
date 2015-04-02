#pragma once
#include <QObject>
#include <iscore/command/OngoingCommandManager.hpp>
#include <Document/Event/EventData.hpp>
#include <QStateMachine>
class TemporalScenarioPresenter;

namespace iscore
{
    class SerializableCommand;
}

struct ConstraintData;
class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class QPointF;

#include "StateMachines/CreateEventStateMachine.hpp"
class ScenarioCommandManager : public QObject
{
    public:
        ScenarioCommandManager(TemporalScenarioPresenter& presenter);

        void setupEventPresenter(EventPresenter* e);
        void setupTimeNodePresenter(TimeNodePresenter* t);
        void setupConstraintPresenter(TemporalConstraintPresenter* c);

        void createConstraint(EventData);
        void on_scenarioPressed(QPointF point, QPointF scenePoint);
        void on_scenarioMoved(QPointF point);
        void on_scenarioReleased(QPointF point, QPointF scenePoint);

        // Moving
        void moveEventAndConstraint(EventData data);
        void moveConstraint(ConstraintData data);
        void moveTimeNode(EventData data);

        void on_ctrlStateChanged(bool);

        // Utility
        bool ongoingCommand();

    private:
        EventData m_lastData {};

        TemporalScenarioPresenter& m_presenter;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;
        LockingOngoingCommandDispatcher<MergeStrategy::Simple>* m_creationCommandDispatcher{};
        LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo>* m_moveCommandDispatcher{};
        CommandDispatcher<SendStrategy::Simple>* m_instantCommandDispatcher{};


        CreateEventState* m_createEvent;

        QStateMachine m_sm;
};
