#pragma once
#include "Tool.hpp"
#include <Process/ExpandMode.hpp>
#include "ScenarioStateMachineBaseEvents.hpp"
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <Scenario/Control/ScenarioEditionSettings.hpp>
#include <QStateMachine>
#include <QPointF>

#include <Scenario/Process/Temporal/StateMachines/Tools/CreationToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/MoveSlotToolState.hpp>

class TemporalScenarioPresenter;
class ScenarioModel;
class QGraphicsScene;

namespace iscore
{
    class CommandStack;
    class ObjectLocker;
    class Document;
}

namespace Scenario
{
class ToolPalette final : public GraphicsSceneToolPalette
{
    public:
        ToolPalette(iscore::Document&, TemporalScenarioPresenter& presenter);

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

    private:
        TemporalScenarioPresenter& m_presenter;
        const ScenarioModel& m_model;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;

        CreationTool createTool;
        SelectionAndMoveTool selectTool;
        MoveSlotTool moveSlotTool;
};

}
