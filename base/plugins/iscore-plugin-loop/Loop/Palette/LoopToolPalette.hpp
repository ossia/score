#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <QPoint>

#include <Scenario/Palette/Tool.hpp>

namespace Scenario {
class EditionSettings;
}  // namespace Scenario
struct LayerContext;

namespace Scenario
{
class MoveConstraintInBaseScenario_StateWrapper;
class MoveLeftBraceInBaseScenario_StateWrapper;
class MoveRightBraceInBaseScenario_StateWrapper;
class MoveEventInBaseScenario_StateWrapper;
class MoveTimeNodeInBaseScenario_StateWrapper;
}
namespace Loop
{
class ProcessModel;
class LayerPresenter;
class LayerView;

class ToolPalette final : public GraphicsSceneToolPalette
{
    public:
        ToolPalette(
                const Loop::ProcessModel& model,
                LayerPresenter& presenter,
                LayerContext& ctx,
                LayerView& view);

        LayerView& view() const;

        const LayerPresenter& presenter() const;
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
        LayerPresenter& m_presenter;
        LayerContext& m_context;
        LayerView& m_view;
        const Scenario::EditionSettings& m_editionSettings;

        Scenario::SmartTool<
            Loop::ProcessModel,
            ToolPalette,
            LayerView,
            Scenario::MoveConstraintInBaseScenario_StateWrapper,
            Scenario::MoveLeftBraceInBaseScenario_StateWrapper,
            Scenario::MoveRightBraceInBaseScenario_StateWrapper,
            Scenario::MoveEventInBaseScenario_StateWrapper,
            Scenario::MoveTimeNodeInBaseScenario_StateWrapper
        >  m_state;

        ToolPaletteInputDispatcher<
               Scenario::Tool,
               ToolPalette,
               LayerContext,
               LayerPresenter
            > m_inputDisp;
};
}
