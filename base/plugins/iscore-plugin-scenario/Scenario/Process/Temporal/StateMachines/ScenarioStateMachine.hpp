#pragma once
#include "Tool.hpp"
#include <Process/ExpandMode.hpp>
#include "ScenarioStateMachineBaseEvents.hpp"
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <QStateMachine>
#include <QPointF>

#include <Scenario/Process/Temporal/StateMachines/Tools/CreationToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/MoveSlotToolState.hpp>

#include <Process/Tools/ToolPalette.hpp>

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

    private:
        Scenario::Point ScenePointToScenarioPoint(QPointF point);
        void changeTool(Scenario::Tool);
        TemporalScenarioPresenter& m_presenter;
        const ScenarioModel& m_model;
        LayerContext& m_context;

        CreationTool<ScenarioModel, Scenario::ToolPalette> m_createTool;
        SelectionAndMoveTool m_selectTool;
        MoveSlotTool m_moveSlotTool;

        ToolPaletteInputDispatcher<
            Scenario::Tool,
            ToolPalette,
            LayerContext,
            TemporalScenarioView> m_inputDisp;
};

}
