#pragma once
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/ScenarioDocument/Widgets/GraphicsProxyObject.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElements_StateWrappers.hpp>

#include <Process/Tools/ToolPalette.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>

#include <Loop/LoopLayer.hpp>
#include <Loop/LoopView.hpp>
class ScenarioDocumentPresenter;
class BaseGraphicsObject;
class DisplayedElementsPresenter;
class DisplayedElementsModel;

class LoopPresenter;
class LoopView;

namespace Loop
{
class ProcessModel;
}
class LoopToolPalette final : public GraphicsSceneToolPalette
{
    public:
        LoopToolPalette(
                const Loop::ProcessModel& model,
                LoopPresenter& presenter,
                LayerContext& ctx,
                LoopView& view);

        LoopView& view() const;

        const LoopPresenter& presenter() const;
        const Loop::ProcessModel& model() const;
        const LayerContext& context() const;
        const Scenario::EditionSettings& editionSettings() const;

        void activate(Scenario::Tool);
        void desactivate(Scenario::Tool);
        void on_pressed(QPointF point);
        void on_moved(QPointF point);
        void on_released(QPointF point);
        void on_cancel();

    private:
        Scenario::Point ScenePointToScenarioPoint(QPointF point);

        const Loop::ProcessModel& m_model;
        LoopPresenter& m_presenter;
        LayerContext& m_context;
        LoopView& m_view;
        const Scenario::EditionSettings& m_editionSettings;

        Scenario::SmartTool<
            Loop::ProcessModel,
            LoopToolPalette,
            LoopView,
            MoveConstraintInBaseScenario_StateWrapper,
            MoveEventInBaseScenario_StateWrapper,
            MoveTimeNodeInBaseScenario_StateWrapper
        >  m_state;

        ToolPaletteInputDispatcher<
               Scenario::Tool,
               LoopToolPalette,
               LayerContext,
               LoopPresenter
            > m_inputDisp;
};

