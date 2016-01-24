#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <QPoint>

#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <iscore/document/DocumentContext.hpp>

class BaseGraphicsObject;

namespace Scenario
{
class EditionSettings;
class MoveConstraintInScenario_StateWrapper;
class MoveBraceInScenario_StateWrapper;
class MoveEventInScenario_StateWrapper;
class MoveTimeNodeInScenario_StateWrapper;
class ScenarioModel;
class DisplayedElementsModel;
class DisplayedElementsPresenter;
class ScenarioDocumentPresenter;

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
        const Scenario::ScenarioModel& m_scenarioModel;
        ScenarioDocumentPresenter& m_presenter;
        BaseElementContext m_context;
        BaseGraphicsObject& m_view;
        const Scenario::EditionSettings& m_editionSettings;

        Scenario::SmartTool<
            Scenario::ScenarioModel,
            ScenarioDisplayedElementsToolPalette,
            BaseGraphicsObject,
            Scenario::MoveConstraintInScenario_StateWrapper,
            Scenario::MoveBraceInScenario_StateWrapper,
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
}
