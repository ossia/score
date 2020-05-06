// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDisplayedElementsToolPalette.hpp"

#include <Process/TimeValue.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElements_StateWrappers.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Palette/Tools/States/ScenarioMoveStatesWrapper.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>
#include <score/tools/std/Optional.hpp>


namespace Scenario
{
class EditionSettings;

Scenario::Point
ScenarioDisplayedElementsToolPalette::ScenePointToScenarioPoint(QPointF point)
{
  return {TimeVal::fromPixels(point.x(), m_presenter.zoomRatio())
              + m_presenter.presenters().startTimeSync().date(),
          0};
}

ScenarioDisplayedElementsToolPalette::ScenarioDisplayedElementsToolPalette(
    const DisplayedElementsModel& model,
    ScenarioDocumentPresenter& pres,
    QGraphicsItem* view)
    : GraphicsSceneToolPalette{*view->scene()}
    , m_model{model}
    , m_scenarioModel{*safe_cast<Scenario::ProcessModel*>(
          m_model.interval().parent())}
    , m_presenter{pres}
    , m_context{pres.context(), m_presenter}
    , m_magnetic{(Process::MagnetismAdjuster&)m_context.context.app.interfaces<Process::MagnetismAdjuster>()}
    , m_view{*view}
    , m_editionSettings{m_context.context.app
                            .guiApplicationPlugin<ScenarioApplicationPlugin>()
                            .editionSettings()}
    , m_state{*this}
    , m_inputDisp{m_presenter, *this, m_context}
{
}

QGraphicsItem& ScenarioDisplayedElementsToolPalette::view() const
{
  return m_view;
}

const DisplayedElementsPresenter&
ScenarioDisplayedElementsToolPalette::presenter() const
{
  return m_presenter.presenters();
}

const Scenario::ProcessModel&
ScenarioDisplayedElementsToolPalette::model() const
{
  return m_scenarioModel;
}

const BaseElementContext& ScenarioDisplayedElementsToolPalette::context() const
{
  return m_context;
}

const Scenario::EditionSettings&
ScenarioDisplayedElementsToolPalette::editionSettings() const
{
  return m_editionSettings;
}

void ScenarioDisplayedElementsToolPalette::activate(Scenario::Tool) {}

void ScenarioDisplayedElementsToolPalette::desactivate(Scenario::Tool) {}

void ScenarioDisplayedElementsToolPalette::on_pressed(QPointF point)
{
  scenePoint = point;
  m_state.on_pressed(
      point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void ScenarioDisplayedElementsToolPalette::on_moved(QPointF point)
{
  scenePoint = point;
  m_state.on_moved(
      point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void ScenarioDisplayedElementsToolPalette::on_released(QPointF point)
{
  scenePoint = point;
  m_state.on_released(
      point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void ScenarioDisplayedElementsToolPalette::on_cancel()
{
  m_state.on_cancel();
}
}
