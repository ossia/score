#pragma once
#include "Tool.hpp"
#include <Process/ExpandMode.hpp>
#include "ScenarioStateMachineBaseEvents.hpp"
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <Scenario/Control/ScenarioEditionSettings.hpp>
#include <QStateMachine>
#include <QPointF>

#include "Tools/CreationToolState.hpp"
#include "Tools/SelectionToolState.hpp"
#include "Tools/MoveSlotToolState.hpp"

class TemporalScenarioPresenter;
class ScenarioModel;
class CreationToolState;
class MoveSlotToolState;
class QGraphicsScene;

namespace iscore
{
    class CommandStack;
    class ObjectLocker;
    class Document;
}

namespace Scenario
{
// TODO namespace Scenario everywhere.
class SelectionAndMoveTool;
}
class ScenarioStateMachine final : public BaseStateMachine
{
        Q_OBJECT
    public:
        ScenarioStateMachine(iscore::Document&, TemporalScenarioPresenter& presenter);

        const TemporalScenarioPresenter& presenter() const
        { return m_presenter; }
        const ScenarioEditionSettings& editionSettings() const;

        const ScenarioModel& model() const
        { return m_model; }

        iscore::CommandStack& commandStack() const
        { return m_commandStack; }
        iscore::ObjectLocker& locker() const
        { return m_locker; }

        void changeTool(ScenarioToolKind);

        ScenarioPoint scenarioPoint;

    signals:
        void exitState();

    private:
        TemporalScenarioPresenter& m_presenter;
        const ScenarioModel& m_model;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;

        CreationToolState createTool;
        Scenario::SelectionAndMoveTool selectTool;
        MoveSlotToolState moveSlotTool;
        QState* playState{};
        QState* transitionState{};


        QState* scaleState{};
        QState* growState{};
        QState* fixedState{};
};
