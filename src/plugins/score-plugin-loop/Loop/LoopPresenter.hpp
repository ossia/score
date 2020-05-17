#pragma once
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopViewUpdater.hpp>
#include <Loop/Palette/LoopToolPalette.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Identifier.hpp>

#include <QPoint>

#include <verdigris>

namespace Loop
{
class LayerPresenter final
    : public Process::LayerPresenter,
      public BaseScenarioPresenter<Loop::ProcessModel, Scenario::TemporalIntervalPresenter>,
      public Nano::Observer
{
  W_OBJECT(LayerPresenter)
  friend class ViewUpdater;

public:
  LayerPresenter(
      const Loop::ProcessModel&,
      LayerView* view,
      const Process::Context& ctx,
      QObject* parent);

  ~LayerPresenter();
  LayerView& view() const { return *m_view; }

  using BaseScenarioPresenter<Loop::ProcessModel, Scenario::TemporalIntervalPresenter>::event;
  using QObject::event;

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;
  void parentGeometryChanged() override;

  void on_addProcess(const Process::ProcessModel& p);
  void on_removeProcess(const Process::ProcessModel& p);
  const Loop::ProcessModel& model() const noexcept;

  ZoomRatio zoomRatio() const { return m_zoomRatio; }

  void
  fillContextMenu(QMenu&, QPoint pos, QPointF scenepos, const Process::LayerContextMenuManager&)
      override;

public:
  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);
  void moved(QPointF arg_1) W_SIGNAL(moved, arg_1);
  void released(QPointF arg_1) W_SIGNAL(released, arg_1);
  void escPressed() W_SIGNAL(escPressed);

private:
  void updateAllElements();
  void on_intervalExecutionTimer();

  graphics_item_ptr<LayerView> m_view;

  ZoomRatio m_zoomRatio{};

  ViewUpdater m_viewUpdater;

  ToolPalette m_palette;
};
}
