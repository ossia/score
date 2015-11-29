#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <qpoint.h>

#include "Scenario/Palette/ScenarioPoint.hpp"
#include "Scenario/Palette/Tool.hpp"
#include "Scenario/Palette/Tools/SmartTool.hpp"

class BaseGraphicsObject;
class BaseScenario;
class DisplayedElementsPresenter;
class MoveConstraintInBaseScenario_StateWrapper;
class MoveEventInBaseScenario_StateWrapper;
class MoveTimeNodeInBaseScenario_StateWrapper;
class ScenarioDocumentPresenter;
namespace Scenario {
class EditionSettings;
}  // namespace Scenario

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

