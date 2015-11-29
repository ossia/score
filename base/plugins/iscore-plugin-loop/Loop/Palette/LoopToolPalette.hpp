#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <QPoint>

#include "Scenario/Palette/Tool.hpp"

class LoopPresenter;
class LoopView;
class MoveConstraintInBaseScenario_StateWrapper;
class MoveEventInBaseScenario_StateWrapper;
class MoveTimeNodeInBaseScenario_StateWrapper;
namespace Scenario {
class EditionSettings;
}  // namespace Scenario
struct LayerContext;

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

