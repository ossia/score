#include <Loop/LoopPresenter.hpp>

#include <algorithm>
#include <iscore/tools/std/Optional.hpp>

#include "Loop/LoopProcessModel.hpp"
#include "Loop/LoopView.hpp"
#include "LoopToolPalette.hpp"
#include <Process/ProcessContext.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElements_StateWrappers.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class EditionSettings;
} // namespace Scenario
namespace Loop
{
ToolPalette::ToolPalette(
    const Loop::ProcessModel& model,
    LayerPresenter& presenter,
    Process::LayerContext& ctx,
    LayerView& view)
    : GraphicsSceneToolPalette{view}
    , m_model{model}
    , m_presenter{presenter}
    , m_context{ctx}
    , m_view{view}
    , m_editionSettings{m_context.context.app.components
                            .applicationPlugin<Scenario::
                                                   ScenarioApplicationPlugin>()
                            .editionSettings()}
    , m_state{*this}
    , m_inputDisp{m_presenter, *this, m_context}
{
}

LayerView& ToolPalette::view() const
{
  return m_view;
}

const LayerPresenter& ToolPalette::presenter() const
{
  return m_presenter;
}

const Loop::ProcessModel& ToolPalette::model() const
{
  return m_model;
}

const Process::LayerContext& ToolPalette::context() const
{
  return m_context;
}

const Scenario::EditionSettings& ToolPalette::editionSettings() const
{
  return m_editionSettings;
}

void ToolPalette::activate(Scenario::Tool)
{
}

void ToolPalette::desactivate(Scenario::Tool)
{
}

void ToolPalette::on_pressed(QPointF point)
{
  scenePoint = point;
  auto scenarioPoint = ScenePointToScenarioPoint(m_view.mapFromScene(point));
  m_state.on_pressed(point, scenarioPoint);
}

void ToolPalette::on_moved(QPointF point)
{
  scenePoint = point;
  auto scenarioPoint = ScenePointToScenarioPoint(m_view.mapFromScene(point));
  m_state.on_moved(point, scenarioPoint);
}

void ToolPalette::on_released(QPointF point)
{
  scenePoint = point;
  auto scenarioPoint = ScenePointToScenarioPoint(m_view.mapFromScene(point));
  m_state.on_released(point, scenarioPoint);
}

void ToolPalette::on_cancel()
{
  m_state.on_cancel();
}

Scenario::Point ToolPalette::ScenePointToScenarioPoint(QPointF point)
{
  return {TimeVal::fromMsecs(point.x() * m_presenter.zoomRatio()), 0};
}
}
