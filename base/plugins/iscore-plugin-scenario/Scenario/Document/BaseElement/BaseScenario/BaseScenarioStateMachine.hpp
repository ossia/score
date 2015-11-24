#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Scenario/Document/BaseElement/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario_StateWrappers.hpp>

#include <Process/Tools/ToolPalette.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>

class DisplayedElementsPresenter;
class DisplayedElementsModel;
class BaseElementPresenter;
class ConstraintModel;

// TODO MoveMe to statemachine folder
// TODO rename me
class BaseScenario;
class BaseGraphicsObject;
class QGraphicsItem;
class ProcessFocusManager;

class BaseScenarioToolPalette final : public GraphicsSceneToolPalette
{
    public:
        BaseScenarioToolPalette(BaseElementPresenter& pres);

        BaseGraphicsObject& view() const;
        const DisplayedElementsPresenter& presenter() const;
        const BaseScenario& model() const;
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

        BaseElementPresenter& m_presenter;
        BaseElementContext m_context;
        Scenario::SelectionAndMoveTool<
                BaseScenario,
                BaseScenarioToolPalette,
                BaseGraphicsObject,
                MoveConstraintInBaseScenario_StateWrapper,
                MoveEventInBaseScenario_StateWrapper,
                MoveTimeNodeInBaseScenario_StateWrapper
            >  m_state;

        ToolPaletteInputDispatcher<
               Scenario::Tool,
               BaseScenarioToolPalette,
               BaseElementContext,
               BaseElementPresenter
            > m_inputDisp;
};

