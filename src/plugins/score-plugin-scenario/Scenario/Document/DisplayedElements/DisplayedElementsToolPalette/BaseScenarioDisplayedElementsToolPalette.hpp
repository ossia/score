#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Palette/Tools/States/ScenarioMoveStatesWrapper.hpp>

#include <score/statemachine/GraphicsSceneToolPalette.hpp>

#include <QPoint>
class BaseGraphicsObject;
namespace Scenario
{
class EditionSettings;

class BaseScenario;
class DisplayedElementsPresenter;
class DoNotMoveInterval_StateWrapper;
class MoveLeftBraceInBaseScenario_StateWrapper;
class MoveRightBraceInBaseScenario_StateWrapper;
class MoveEventInBaseScenario_StateWrapper;
class MoveTimeSyncInBaseScenario_StateWrapper;
class ScenarioDocumentPresenter;

class BaseScenarioDisplayedElementsToolPalette final : public GraphicsSceneToolPalette
{
public:
  BaseScenarioDisplayedElementsToolPalette(ScenarioDocumentPresenter& pres);

  BaseGraphicsObject& view() const;
  DisplayedElementsPresenter& presenter() const;
  const BaseScenario& model() const;
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

  ScenarioDocumentPresenter& m_presenter;
  BaseElementContext m_context;
  Process::MagnetismAdjuster& m_magnetic;

  Scenario::SmartTool<
      BaseScenario,
      BaseScenarioDisplayedElementsToolPalette,
      BaseGraphicsObject,
      DoNotMoveInterval_StateWrapper,
      MoveLeftBraceInScenario_StateWrapper,
      MoveRightBraceInScenario_StateWrapper,
      MoveEventInBaseScenario_StateWrapper,
      MoveTimeSyncInBaseScenario_StateWrapper>
      m_state;

  ToolPaletteInputDispatcher<
      Scenario::Tool,
      BaseScenarioDisplayedElementsToolPalette,
      BaseElementContext,
      ScenarioDocumentPresenter>
      m_inputDisp;
};
}
