#pragma once
#include <Scenario/Process/Temporal/StateMachines/Tool.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseEvents.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>

#include <Scenario/Process/Temporal/StateMachines/Tools/CreationToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/MoveSlotToolState.hpp>

#include <Process/ExpandMode.hpp>
#include <Process/Tools/ToolPalette.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>
#include <QStateMachine>
#include <QPointF>

class TemporalScenarioPresenter;
class TemporalScenarioView;
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
        ToolPalette(LayerContext&, TemporalScenarioPresenter& presenter);

        const TemporalScenarioPresenter& presenter() const
        { return m_presenter; }
        const Scenario::EditionSettings& editionSettings() const;

        const ScenarioModel& model() const
        { return m_model; }

        iscore::CommandStack& commandStack() const
        { return m_context.commandStack; }
        iscore::ObjectLocker& locker() const
        { return m_context.objectLocker; }

        void on_pressed(QPointF);
        void on_moved(QPointF);
        void on_released(QPointF);
        void on_cancel();

        void activate(Scenario::Tool);
        void desactivate(Scenario::Tool);

    private:
        Scenario::Point ScenePointToScenarioPoint(QPointF point);

        TemporalScenarioPresenter& m_presenter;
        const ScenarioModel& m_model;
        LayerContext& m_context;

        CreationTool<ScenarioModel, Scenario::ToolPalette> m_createTool;
        SelectionAndMoveTool<ScenarioModel, Scenario::ToolPalette> m_selectTool;
        MoveSlotTool m_moveSlotTool;

        ToolPaletteInputDispatcher<
            Scenario::Tool,
            ToolPalette,
            LayerContext,
            TemporalScenarioView> m_inputDisp;
};

}
