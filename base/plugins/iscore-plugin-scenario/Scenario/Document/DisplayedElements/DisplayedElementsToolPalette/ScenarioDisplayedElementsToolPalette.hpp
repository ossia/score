#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/ScenarioDocument/Widgets/GraphicsProxyObject.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SmartTool.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/ScenarioMoveStatesWrapper.hpp>

#include <Process/Tools/ToolPalette.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>

class ScenarioDocumentPresenter;
class BaseGraphicsObject;
class DisplayedElementsPresenter;
class DisplayedElementsModel;
namespace Scenario { class ScenarioModel; }

class ScenarioDisplayedElementsToolPalette final : public GraphicsSceneToolPalette
{
    public:
        ScenarioDisplayedElementsToolPalette(
                const iscore::DocumentContext& ctx,
                const DisplayedElementsModel&,
                ScenarioDocumentPresenter&,
                BaseGraphicsObject&);

        BaseGraphicsObject& view() const;
        const DisplayedElementsPresenter& presenter() const;
        const Scenario::ScenarioModel& model() const;
        const BaseElementContext& context() const;
        const Scenario::EditionSettings& editionSettings() const;

        void activate(Scenario::Tool);
        void desactivate(Scenario::Tool);

        void on_pressed(QPointF);
        void on_moved(QPointF);
        void on_released(QPointF);
        void on_cancel();

    private:
        Scenario::Point ScenePointToScenarioPoint(QPointF point);
        const DisplayedElementsModel& m_model;
        ScenarioDocumentPresenter& m_presenter;
        BaseElementContext m_context;
        BaseGraphicsObject& m_view;
        const Scenario::EditionSettings& m_editionSettings;

        Scenario::SmartTool<
            Scenario::ScenarioModel,
            ScenarioDisplayedElementsToolPalette,
            BaseGraphicsObject,
            Scenario::MoveConstraintInScenario_StateWrapper,
            Scenario::MoveEventInScenario_StateWrapper,
            Scenario::MoveTimeNodeInScenario_StateWrapper
        >  m_state;

        ToolPaletteInputDispatcher<
               Scenario::Tool,
               ScenarioDisplayedElementsToolPalette,
               BaseElementContext,
               ScenarioDocumentPresenter
            > m_inputDisp;
};

