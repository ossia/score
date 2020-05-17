#pragma once
#include <Process/ProcessContext.hpp>
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Palette/Tools/CreationToolState.hpp>
#include <Scenario/Palette/Tools/PlayToolState.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>

#include <score/statemachine/GraphicsSceneToolPalette.hpp>

#include <QPoint>

namespace score
{
class CommandStack;
class ObjectLocker;
}

namespace Process
{
class MagnetismAdjuster;
}

namespace Scenario
{
class EditionSettings;
class ScenarioPresenter;
class ScenarioView;
class MoveIntervalInScenario_StateWrapper;
class MoveLeftBraceInScenario_StateWrapper;
class MoveRightBraceInScenario_StateWrapper;
class MoveEventInScenario_StateWrapper;
class MoveTimeSyncInScenario_StateWrapper;
class ProcessModel;
class ToolPalette final : public GraphicsSceneToolPalette
{
public:
  ToolPalette(Process::LayerContext&, ScenarioPresenter& presenter);

  const ScenarioPresenter& presenter() const { return m_presenter; }
  Scenario::EditionSettings& editionSettings() const;

  const Process::LayerContext& context() const { return m_context; }
  Process::MagnetismAdjuster& magnetic() const { return m_magnetic; }

  const Scenario::ProcessModel& model() const { return m_model; }

  void on_pressed(QPointF);
  void on_moved(QPointF);
  void on_released(QPointF);
  void on_cancel();

  void activate(Scenario::Tool);
  void desactivate(Scenario::Tool);

  QGraphicsItem*
  itemAt(const Scenario::Point&, const std::vector<QGraphicsItem*>& ignore) const noexcept;

private:
  Scenario::Point ScenePointToScenarioPoint(QPointF point);

  ScenarioPresenter& m_presenter;
  const Scenario::ProcessModel& m_model;
  Process::LayerContext& m_context;
  Process::MagnetismAdjuster& m_magnetic;

  CreationTool<ProcessModel, Scenario::ToolPalette> m_createTool;
  SmartTool<
      ProcessModel,
      Scenario::ToolPalette,
      ScenarioView,
      Scenario::MoveIntervalInScenario_StateWrapper,
      Scenario::MoveLeftBraceInScenario_StateWrapper,
      Scenario::MoveRightBraceInScenario_StateWrapper,
      Scenario::MoveEventInScenario_StateWrapper,
      Scenario::MoveTimeSyncInScenario_StateWrapper>
      m_selectTool;

  PlayToolState m_playTool;

  ToolPaletteInputDispatcher<
      Scenario::Tool,
      ToolPalette,
      Process::LayerContext,
      Process::LayerView>
      m_inputDisp;
};
}
