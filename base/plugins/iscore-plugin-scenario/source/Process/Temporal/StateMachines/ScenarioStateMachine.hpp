#pragma once
#include "Tool.hpp"
#include <ProcessInterface/ExpandMode.hpp>
#include "ScenarioStateMachineBaseEvents.hpp"


#include <iscore/statemachine/BaseStateMachine.hpp>

#include <QStateMachine>
#include <QPointF>

class TemporalScenarioPresenter;
class ScenarioModel;
class CreationToolState;
class MoveToolState;
class SelectionTool;
class MoveSlotToolState;
class QGraphicsScene;

namespace iscore
{
    class CommandStack;
    class ObjectLocker;
    class Document;
}

class ScenarioStateMachine : public BaseStateMachine
{
        Q_OBJECT
    public:
        ScenarioStateMachine(iscore::Document&, TemporalScenarioPresenter& presenter);

        const TemporalScenarioPresenter& presenter() const;
        const ScenarioModel& model() const { return m_model; }

        iscore::CommandStack& commandStack() const
        { return m_commandStack; }
        iscore::ObjectLocker& locker() const
        { return m_locker; }

        ScenarioToolKind tool() const;
        const ExpandMode& expandMode() const
        { return m_expandMode; }
        bool isShiftPressed() const;

        void changeTool(int);

        ScenarioPoint scenarioPoint;

    signals:
        void setCreateState();
        void setSelectState();
        void setMoveState();
        void setSlotMoveState();
        void setPlayState();
        void exitState();

        void shiftPressed();
        void shiftReleased();

    private:
        TemporalScenarioPresenter& m_presenter;
        const ScenarioModel& m_model;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;

        const ExpandMode& m_expandMode; // Reference to the one in ScenarioControl.

        CreationToolState* createState{};
        MoveToolState* moveState{};
        SelectionTool* selectState{};
        MoveSlotToolState* moveSlotState{};
        QState* playState{};
        QState* transitionState{};


        QState* scaleState{};
        QState* growState{};
        QState* fixedState{};
        QState* shiftReleasedState{};
        QState* shiftPressedState{};
};
