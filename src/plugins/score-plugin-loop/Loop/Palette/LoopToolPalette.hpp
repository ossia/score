#pragma once
#include <Process/Tools/ToolPalette.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Palette/Tools/States/ScenarioMoveStatesWrapper.hpp>

#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>

#include <QPoint>

namespace Scenario
{
class EditionSettings;
} // namespace Scenario
namespace Process
{
struct LayerContext;
}

namespace Scenario
{
class DoNotMoveInterval_StateWrapper;
class MoveEventInBaseScenario_StateWrapper;
class MoveTimeSyncInBaseScenario_StateWrapper;
}
namespace Loop
{
class ProcessModel;
class LayerPresenter;
class LayerView;

class ToolPalette final : public GraphicsSceneToolPalette
{
public:
  ToolPalette(
      const Loop::ProcessModel& model,
      LayerPresenter& presenter,
      Process::LayerContext& ctx,
      LayerView& view);

  LayerView& view() const;

  const LayerPresenter& presenter() const;
  const Loop::ProcessModel& model() const;
  const Process::LayerContext& context() const;
  Process::MagnetismAdjuster& magnetic() const { return m_magnetic; }
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
  LayerPresenter& m_presenter;
  Process::LayerContext& m_context;
  Process::MagnetismAdjuster& m_magnetic;
  LayerView& m_view;
  const Scenario::EditionSettings& m_editionSettings;

  Scenario::SmartTool<
      Loop::ProcessModel,
      ToolPalette,
      LayerView,
      Scenario::DoNotMoveInterval_StateWrapper,
      Scenario::MoveLeftBraceInScenario_StateWrapper,
      Scenario::MoveRightBraceInScenario_StateWrapper,
      Scenario::MoveEventInBaseScenario_StateWrapper,
      Scenario::MoveTimeSyncInBaseScenario_StateWrapper>
      m_state;

  ToolPaletteInputDispatcher<Scenario::Tool, ToolPalette, Process::LayerContext, LayerPresenter>
      m_inputDisp;
};

class DisplayedElementsToolPalette final : public GraphicsSceneToolPalette
{
public:
  DisplayedElementsToolPalette(
      const Scenario::DisplayedElementsModel&,
      Scenario::ScenarioDocumentPresenter&,
      QGraphicsItem*);

  QGraphicsItem& view() const;
  const Scenario::DisplayedElementsPresenter& presenter() const;
  const Loop::ProcessModel& model() const;
  const Scenario::BaseElementContext& context() const;
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
  const Scenario::DisplayedElementsModel& m_model;
  const Loop::ProcessModel& m_scenarioModel;
  Scenario::ScenarioDocumentPresenter& m_presenter;
  Scenario::BaseElementContext m_context;
  Process::MagnetismAdjuster& m_magnetic;
  QGraphicsItem& m_view;
  const Scenario::EditionSettings& m_editionSettings;

  Scenario::SmartTool<
      Loop::ProcessModel,
      DisplayedElementsToolPalette,
      BaseGraphicsObject,
      Scenario::DoNotMoveInterval_StateWrapper,
      Scenario::MoveLeftBraceInScenario_StateWrapper,
      Scenario::MoveRightBraceInScenario_StateWrapper,
      Scenario::MoveEventInBaseScenario_StateWrapper,
      Scenario::MoveTimeSyncInBaseScenario_StateWrapper>
      m_state;

  ToolPaletteInputDispatcher<
      Scenario::Tool,
      DisplayedElementsToolPalette,
      Scenario::BaseElementContext,
      Scenario::ScenarioDocumentPresenter>
      m_inputDisp;
};

class DisplayedElementsToolPaletteFactory final
    : public Scenario::DisplayedElementsToolPaletteFactory
{
  SCORE_CONCRETE("0e5053e8-2165-445e-acb8-3f5715fe25c8")
public:
  bool matches(const Scenario::IntervalModel& interval) const override;

  std::unique_ptr<GraphicsSceneToolPalette> make(
      Scenario::ScenarioDocumentPresenter& pres,
      const Scenario::IntervalModel& interval,
      QGraphicsItem* parent) override;
};
}
