#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>

#include <QPoint>

class BaseGraphicsObject;

namespace Process
{
class MagnetismAdjuster;
}

namespace Scenario
{
class EditionSettings;
class DoNotMoveInterval_StateWrapper;
class MoveLeftBraceInScenario_StateWrapper;
class MoveRightBraceInScenario_StateWrapper;
class MoveEventInTopScenario_StateWrapper;
class MoveTimeSyncInTopScenario_StateWrapper;
class ProcessModel;
class DisplayedElementsModel;
class DisplayedElementsPresenter;
class ScenarioDocumentPresenter;

class ScenarioDisplayedElementsToolPalette final : public GraphicsSceneToolPalette
{
public:
  ScenarioDisplayedElementsToolPalette(
      const DisplayedElementsModel&,
      ScenarioDocumentPresenter&,
      QGraphicsItem*);

  QGraphicsItem& view() const;
  const DisplayedElementsPresenter& presenter() const;
  const Scenario::ProcessModel& model() const;
  const BaseElementContext& context() const;
  Process::MagnetismAdjuster& magnetic() const { return m_magnetic; }
  const Scenario::EditionSettings& editionSettings() const;

  void activate(Scenario::Tool);
  void desactivate(Scenario::Tool);

  void on_pressed(QPointF);
  void on_moved(QPointF);
  void on_released(QPointF);
  void on_cancel();

private:
  Scenario::Point ScenePointToScenarioPoint(QPointF point);
  const DisplayedElementsModel& m_model;
  const Scenario::ProcessModel& m_scenarioModel;
  ScenarioDocumentPresenter& m_presenter;
  BaseElementContext m_context;
  Process::MagnetismAdjuster& m_magnetic;
  QGraphicsItem& m_view;
  const Scenario::EditionSettings& m_editionSettings;

  Scenario::SmartTool<
      Scenario::ProcessModel,
      ScenarioDisplayedElementsToolPalette,
      BaseGraphicsObject,
      Scenario::DoNotMoveInterval_StateWrapper,
      Scenario::MoveLeftBraceInScenario_StateWrapper,
      Scenario::MoveRightBraceInScenario_StateWrapper,
      Scenario::MoveEventInTopScenario_StateWrapper,
      Scenario::MoveTimeSyncInTopScenario_StateWrapper>
      m_state;

  ToolPaletteInputDispatcher<
      Scenario::Tool,
      ScenarioDisplayedElementsToolPalette,
      BaseElementContext,
      ScenarioDocumentPresenter>
      m_inputDisp;
};
}
