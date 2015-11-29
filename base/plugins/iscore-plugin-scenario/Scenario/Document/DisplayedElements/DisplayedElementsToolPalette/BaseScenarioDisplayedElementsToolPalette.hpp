#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElements_StateWrappers.hpp>

#include <Process/Tools/ToolPalette.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>

class DisplayedElementsPresenter;
class DisplayedElementsModel;
class ScenarioDocumentPresenter;
class ConstraintModel;
class BaseScenario;
class BaseGraphicsObject;
class QGraphicsItem;
class ProcessFocusManager;

class BaseScenarioDisplayedElementsToolPalette final : public GraphicsSceneToolPalette
{
    public:
        BaseScenarioDisplayedElementsToolPalette(ScenarioDocumentPresenter& pres);

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

        ScenarioDocumentPresenter& m_presenter;
        BaseElementContext m_context;
        Scenario::SmartTool<
                BaseScenario,
                BaseScenarioDisplayedElementsToolPalette,
                BaseGraphicsObject,
                MoveConstraintInBaseScenario_StateWrapper,
                MoveEventInBaseScenario_StateWrapper,
                MoveTimeNodeInBaseScenario_StateWrapper
            >  m_state;

        ToolPaletteInputDispatcher<
               Scenario::Tool,
               BaseScenarioDisplayedElementsToolPalette,
               BaseElementContext,
               ScenarioDocumentPresenter
            > m_inputDisp;
};

