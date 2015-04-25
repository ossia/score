#pragma once

#include "StateMachineVeryCommon.hpp"

#include <iscore/command/OngoingCommandManager.hpp>

#include <QStateMachine>

class TemporalScenarioPresenter;
class ScenarioModel;
class CreationToolState;
class MoveToolState;
class SelectionToolState;
class MoveDeckToolState;
class ScenarioStateMachine : public QStateMachine
{
        Q_OBJECT
    public:
        enum class State { Create, Select, Move, MoveDeck };
        QPointF scenePoint;
        ScenarioPoint scenarioPoint;

        ScenarioStateMachine(TemporalScenarioPresenter& presenter);

        const TemporalScenarioPresenter& presenter() const
        { return m_presenter; }
        const ScenarioModel& model() const;

        iscore::CommandStack& commandStack() const
        { return m_commandStack; }
        iscore::ObjectLocker& locker() const
        { return m_locker; }

        State currentState() const;

    signals:
        void setCreateState();
        void setSelectState();
        void setMoveState();
        void setDeckMoveState();

    private:
        TemporalScenarioPresenter& m_presenter;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;

        CreationToolState* createState{};
        MoveToolState* moveState{};
        SelectionToolState* selectState{};
        MoveDeckToolState* moveDeckState{};
};
