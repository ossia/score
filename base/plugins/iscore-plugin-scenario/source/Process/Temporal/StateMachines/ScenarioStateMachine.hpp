#pragma once
#include "Tool.hpp"
#include <ProcessInterface/ExpandMode.hpp>
#include "ScenarioStateMachineBaseEvents.hpp"

#include <iscore/command/OngoingCommandManager.hpp>
#include <iscore/statemachine/BaseStateMachine.hpp>

#include <QStateMachine>
#include <QPointF>

class TemporalScenarioPresenter;
class ScenarioModel;
class CreationToolState;
class MoveToolState;
class SelectionTool;
class MoveDeckToolState;
class QGraphicsScene;


class ScenarioStateMachine : public BaseStateMachine
{
        Q_OBJECT
    public:
        ScenarioStateMachine(TemporalScenarioPresenter& presenter);

        const TemporalScenarioPresenter& presenter() const;
        const ScenarioModel& model() const;

        iscore::CommandStack& commandStack() const
        { return m_commandStack; }
        iscore::ObjectLocker& locker() const
        { return m_locker; }

        Tool tool() const;
        ExpandMode expandMode() const;
        bool isShiftPressed() const;

        void changeTool(int);

        ScenarioPoint scenarioPoint;

    signals:
        void setCreateState();
        void setSelectState();
        void setMoveState();
        void setDeckMoveState();
        void exitState();

        void setScaleState();
        void setGrowState();
        void setFixedState();

        void shiftPressed();
        void shiftReleased();

    private:
        TemporalScenarioPresenter& m_presenter;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;

        CreationToolState* createState{};
        MoveToolState* moveState{};
        SelectionTool* selectState{};
        MoveDeckToolState* moveDeckState{};
        QState* transitionState{};

        QState* scaleState{};
        QState* growState{};
        QState* fixedState{};
        QState* shiftReleasedState{};
        QState* shiftPressedState{};
};
