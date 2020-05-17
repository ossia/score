// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopToolPalette.hpp"

#include "Loop/LoopProcessModel.hpp"
#include "Loop/LoopView.hpp"

#include <Loop/LoopPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElements_StateWrappers.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Palette/Tools/States/ScenarioMoveStatesWrapper.hpp>
#include <Magnetism/MagnetismAdjuster.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>
#include <score/tools/std/Optional.hpp>


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
    : GraphicsSceneToolPalette{*view.scene()}
    , m_model{model}
    , m_presenter{presenter}
    , m_context{ctx}
    , m_magnetic{(Process::MagnetismAdjuster&)m_context.context.app.interfaces<Process::MagnetismAdjuster>()}
    , m_view{view}
    , m_editionSettings{m_context.context.app
                            .guiApplicationPlugin<
                                Scenario::ScenarioApplicationPlugin>()
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

void ToolPalette::activate(Scenario::Tool) {}

void ToolPalette::desactivate(Scenario::Tool) {}

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
  return {TimeVal::fromPixels(point.x(), m_presenter.zoomRatio()), 0};
}

Scenario::Point
DisplayedElementsToolPalette::ScenePointToScenarioPoint(QPointF point)
{
  return {TimeVal::fromPixels(point.x(), m_presenter.zoomRatio()), 0};
}

DisplayedElementsToolPalette::DisplayedElementsToolPalette(
    const Scenario::DisplayedElementsModel& model,
    Scenario::ScenarioDocumentPresenter& pres,
    QGraphicsItem* view)
    : GraphicsSceneToolPalette{*view->scene()}
    , m_model{model}
    , m_scenarioModel{*safe_cast<Loop::ProcessModel*>(
          m_model.interval().parent())}
    , m_presenter{pres}
    , m_context{pres.context(), m_presenter}
    , m_magnetic{(Process::MagnetismAdjuster&)m_context.context.app.interfaces<Process::MagnetismAdjuster>()}
    , m_view{*view}
    , m_editionSettings{m_context.context.app
                            .guiApplicationPlugin<
                                Scenario::ScenarioApplicationPlugin>()
                            .editionSettings()}
    , m_state{*this}
    , m_inputDisp{m_presenter, *this, m_context}
{
}

QGraphicsItem& DisplayedElementsToolPalette::view() const
{
  return m_view;
}

const Scenario::DisplayedElementsPresenter&
DisplayedElementsToolPalette::presenter() const
{
  return m_presenter.presenters();
}

const Loop::ProcessModel& DisplayedElementsToolPalette::model() const
{
  return m_scenarioModel;
}

const Scenario::BaseElementContext&
DisplayedElementsToolPalette::context() const
{
  return m_context;
}

const Scenario::EditionSettings&
DisplayedElementsToolPalette::editionSettings() const
{
  return m_editionSettings;
}

void DisplayedElementsToolPalette::activate(Scenario::Tool) {}

void DisplayedElementsToolPalette::desactivate(Scenario::Tool) {}

void DisplayedElementsToolPalette::on_pressed(QPointF point)
{
  scenePoint = point;
  m_state.on_pressed(
      point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void DisplayedElementsToolPalette::on_moved(QPointF point)
{
  scenePoint = point;
  m_state.on_moved(
      point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void DisplayedElementsToolPalette::on_released(QPointF point)
{
  scenePoint = point;
  m_state.on_released(
      point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void DisplayedElementsToolPalette::on_cancel()
{
  m_state.on_cancel();
}
std::unique_ptr<GraphicsSceneToolPalette>
DisplayedElementsToolPaletteFactory::make(
    Scenario::ScenarioDocumentPresenter& pres,
    const Scenario::IntervalModel& interval,
    QGraphicsItem* parent)
{
  return std::make_unique<DisplayedElementsToolPalette>(
      pres.displayedElements, pres, parent);
}

bool DisplayedElementsToolPaletteFactory::matches(
    const Scenario::IntervalModel& interval) const
{
  return dynamic_cast<Loop::ProcessModel*>(interval.parent());
}
}
