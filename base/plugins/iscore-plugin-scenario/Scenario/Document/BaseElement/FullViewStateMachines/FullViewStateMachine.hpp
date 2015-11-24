#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/BaseElement/Widgets/GraphicsProxyObject.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/ScenarioMoveStatesWrapper.hpp>

#include <Process/Tools/ToolPalette.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>

class BaseElementPresenter;
class BaseGraphicsObject;
class DisplayedElementsPresenter;
class DisplayedElementsModel;
class ScenarioModel;

// RENAME FILE
class FullViewToolPalette final : public GraphicsSceneToolPalette
{
    public:
        FullViewToolPalette(
                const iscore::DocumentContext& ctx,
                const DisplayedElementsModel&,
                BaseElementPresenter&,
                BaseGraphicsObject&);

        BaseGraphicsObject& view() const;
        const DisplayedElementsPresenter& presenter() const;
        const ScenarioModel& model() const;
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
        BaseElementPresenter& m_presenter;
        BaseElementContext m_context;
        BaseGraphicsObject& m_view;
        const Scenario::EditionSettings& m_editionSettings;

        Scenario::SelectionAndMoveTool<
            ScenarioModel,
            FullViewToolPalette,
            BaseGraphicsObject,
            Scenario::MoveConstraintInScenario_StateWrapper,
            Scenario::MoveEventInScenario_StateWrapper,
            Scenario::MoveTimeNodeInScenario_StateWrapper
        >  m_state;

        ToolPaletteInputDispatcher<
               Scenario::Tool,
               FullViewToolPalette,
               BaseElementContext,
               BaseElementPresenter
            > m_inputDisp;
};

