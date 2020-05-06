// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BaseScenarioDisplayedElementsToolPalette.hpp"

#include <Process/TimeValue.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElements_StateWrappers.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>
#include <score/tools/std/Optional.hpp>


namespace Scenario
{
class EditionSettings;

Scenario::Point
BaseScenarioDisplayedElementsToolPalette::ScenePointToScenarioPoint(
    QPointF point)
{
  return {TimeVal::fromPixels(point.x(), m_presenter.zoomRatio()), 0};
}

// We need two tool palettes : one for the case where we're viewing a
// basescenario,
// and one for the case where we're in a sub-scenario.
BaseScenarioDisplayedElementsToolPalette::
    BaseScenarioDisplayedElementsToolPalette(ScenarioDocumentPresenter& pres)
    : GraphicsSceneToolPalette{pres.view().scene()}
    , m_presenter{pres}
    , m_context{pres.context(), m_presenter}
    , m_magnetic{(Process::MagnetismAdjuster&)m_context.context.app.interfaces<Process::MagnetismAdjuster>()}
    , m_state{*this}
    , m_inputDisp{m_presenter, *this, m_context}
{
}

BaseGraphicsObject& BaseScenarioDisplayedElementsToolPalette::view() const
{
  return m_presenter.view().baseItem();
}

const DisplayedElementsPresenter&
BaseScenarioDisplayedElementsToolPalette::presenter() const
{
  return m_presenter.presenters();
}

const BaseScenario& BaseScenarioDisplayedElementsToolPalette::model() const
{
  return m_presenter.model().baseScenario();
}

const BaseElementContext&
BaseScenarioDisplayedElementsToolPalette::context() const
{
  return m_context;
}

const Scenario::EditionSettings&
BaseScenarioDisplayedElementsToolPalette::editionSettings() const
{
  return m_context.context.app
      .guiApplicationPlugin<ScenarioApplicationPlugin>()
      .editionSettings(); // OPTIMIZEME
}

void BaseScenarioDisplayedElementsToolPalette::activate(Scenario::Tool) {}

void BaseScenarioDisplayedElementsToolPalette::desactivate(Scenario::Tool) {}

void BaseScenarioDisplayedElementsToolPalette::on_pressed(QPointF point)
{
  scenePoint = point;
  m_state.on_pressed(
      point,
      ScenePointToScenarioPoint(
          m_presenter.view().baseItem().mapFromScene(point)));
}

void BaseScenarioDisplayedElementsToolPalette::on_moved(QPointF point)
{
  scenePoint = point;
  m_state.on_moved(
      point,
      ScenePointToScenarioPoint(
          m_presenter.view().baseItem().mapFromScene(point)));
}

void BaseScenarioDisplayedElementsToolPalette::on_released(QPointF point)
{
  scenePoint = point;
  m_state.on_released(
      point,
      ScenePointToScenarioPoint(
          m_presenter.view().baseItem().mapFromScene(point)));
}

void BaseScenarioDisplayedElementsToolPalette::on_cancel()
{
  m_state.on_cancel();
}
}
