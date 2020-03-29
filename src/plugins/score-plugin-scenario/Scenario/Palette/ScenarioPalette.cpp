// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ScenarioPalette.hpp"

#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Palette/Tools/CreationToolState.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Palette/Tools/States/ScenarioMoveStatesWrapper.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <Magnetism/MagnetismAdjuster.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>
#include <score/tools/std/Optional.hpp>


#include <vector>

namespace Scenario
{
SlotState::~SlotState() {}
ToolPalette::ToolPalette(
    Process::LayerContext& lay,
    ScenarioPresenter& presenter)
    : GraphicsSceneToolPalette{*presenter.view().scene()}
    , m_presenter{presenter}
    , m_model{m_presenter.model()}
    , m_context{lay}
    , m_magnetic{(Process::MagnetismAdjuster&)lay.context.app.interfaces<Process::MagnetismAdjuster>()}
    , m_createTool{*this}
    , m_selectTool{*this}
    , m_playTool{*this}
    , m_inputDisp{presenter.view(), *this, lay}
{
}

Scenario::EditionSettings& ToolPalette::editionSettings() const
{
  return m_presenter.editionSettings();
}

void ToolPalette::on_pressed(QPointF point)
{
  scenePoint = point;
  auto scenarioPoint
      = ScenePointToScenarioPoint(m_presenter.m_view->mapFromScene(point));
  switch (editionSettings().tool())
  {
    case Scenario::Tool::Create:
      m_createTool.on_pressed(point, scenarioPoint);
      break;
    case Scenario::Tool::Playing:
    case Scenario::Tool::Select:
      m_selectTool.on_pressed(point, scenarioPoint);
      break;
    case Scenario::Tool::Play:
      m_playTool.on_pressed(point, scenarioPoint);
      break;
    default:
      break;
  }
}

void ToolPalette::on_moved(QPointF point)
{
  scenePoint = point;
  auto scenarioPoint
      = ScenePointToScenarioPoint(m_presenter.m_view->mapFromScene(point));
  switch (editionSettings().tool())
  {
    case Scenario::Tool::Create:
      m_createTool.on_moved(point, scenarioPoint);
      break;
    case Scenario::Tool::Select:
      m_selectTool.on_moved(point, scenarioPoint);
      break;
    default:
      break;
  }
}

void ToolPalette::on_released(QPointF point)
{
  scenePoint = point;
  auto& es = m_presenter.editionSettings();
  auto scenarioPoint
      = ScenePointToScenarioPoint(m_presenter.m_view->mapFromScene(point));
  switch (es.tool())
  {
    case Scenario::Tool::Create:
      m_createTool.on_released(point, scenarioPoint);
      es.setTool(Scenario::Tool::Select);
      break;
    case Scenario::Tool::Playing:
      m_selectTool.on_released(point, scenarioPoint);
      es.setTool(Scenario::Tool::Select);
      break;
    case Scenario::Tool::Select:
      m_selectTool.on_released(point, scenarioPoint);
      break;
    default:
      break;
  }
}

void ToolPalette::on_cancel()
{
  m_createTool.on_cancel();
  m_selectTool.on_cancel();
  m_presenter.editionSettings().setTool(Scenario::Tool::Select);
}

void ToolPalette::activate(Tool t) {}

void ToolPalette::desactivate(Tool t) {}

Scenario::Point ToolPalette::ScenePointToScenarioPoint(QPointF point)
{
  return ConvertToScenarioPoint(
      point,
      m_presenter.zoomRatio(),
      m_presenter.view().boundingRect().height());
}
}
