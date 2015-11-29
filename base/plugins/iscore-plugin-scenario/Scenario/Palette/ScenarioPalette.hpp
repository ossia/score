#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Palette/Tools/CreationToolState.hpp>
#include <Scenario/Palette/Tools/MoveSlotToolState.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <qpoint.h>

#include "Process/ProcessContext.hpp"
#include "Scenario/Palette/ScenarioPoint.hpp"

class TemporalScenarioPresenter;
class TemporalScenarioView;
namespace Scenario {
class EditionSettings;
class MoveConstraintInScenario_StateWrapper;
class MoveEventInScenario_StateWrapper;
class MoveTimeNodeInScenario_StateWrapper;
class ScenarioModel;
}  // namespace Scenario

namespace iscore
{
    class CommandStack;
    class ObjectLocker;
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

        const LayerContext& context() const
        { return m_context; }

        const Scenario::ScenarioModel& model() const
        { return m_model; }

        iscore::CommandStack& commandStack() const
        { return m_context.commandStack; }
        iscore::ObjectLocker& locker() const
        { return m_context.objectLocker; }

        void on_pressed(QPointF);
        void on_moved(QPointF);
        void on_released(QPointF);
        void on_cancel();

        void activate(Scenario::Tool);
        void desactivate(Scenario::Tool);

    private:
        Scenario::Point ScenePointToScenarioPoint(QPointF point);

        TemporalScenarioPresenter& m_presenter;
        const Scenario::ScenarioModel& m_model;
        LayerContext& m_context;

        CreationTool<ScenarioModel, Scenario::ToolPalette> m_createTool;
        SmartTool<
            ScenarioModel,
            Scenario::ToolPalette,
            TemporalScenarioView,
            Scenario::MoveConstraintInScenario_StateWrapper,
            Scenario::MoveEventInScenario_StateWrapper,
            Scenario::MoveTimeNodeInScenario_StateWrapper> m_selectTool;
        MoveSlotTool m_moveSlotTool;

        ToolPaletteInputDispatcher<
            Scenario::Tool,
            ToolPalette,
            LayerContext,
            TemporalScenarioView> m_inputDisp;
};

}
