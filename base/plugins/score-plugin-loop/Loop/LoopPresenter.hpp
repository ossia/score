#pragma once
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopViewUpdater.hpp>
#include <Loop/Palette/LoopToolPalette.hpp>
#include <Process/LayerPresenter.hpp>
#include <QDebug>
#include <QPoint>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <score/model/Identifier.hpp>
#include <score/widgets/GraphicsItem.hpp>


namespace Process
{
class ProcessModel;
}
class QMenu;
class QObject;
namespace Scenario
{
class TemporalIntervalPresenter;
}
namespace Loop
{
class LayerView;
class ProcessModel;
} // namespace Loop
namespace score
{
class CommandStackFacade;
struct DocumentContext;
} // namespace score

namespace Loop
{
inline void removeSelection(
    const Loop::ProcessModel& model, const score::CommandStackFacade&)
{
}
void clearContentFromSelection(
    const Loop::ProcessModel& model, const score::CommandStackFacade&);
}

namespace Loop
{
class LayerPresenter final
    : public Process::LayerPresenter,
      public BaseScenarioPresenter<Loop::ProcessModel, Scenario::TemporalIntervalPresenter>
{
  Q_OBJECT
  friend class ViewUpdater;

public:
  LayerPresenter(
      const Loop::ProcessModel&,
      LayerView* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent);

  ~LayerPresenter();
  LayerView& view() const
  {
    return *m_view;
  }

  using BaseScenarioPresenter<Loop::ProcessModel, Scenario::TemporalIntervalPresenter>::
      event;
  using QObject::event;

  void setWidth(qreal width) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;
  void parentGeometryChanged() override;

  const Loop::ProcessModel& model() const override;
  const Id<Process::ProcessModel>& modelId() const override;

  ZoomRatio zoomRatio() const
  {
    return m_zoomRatio;
  }

  void fillContextMenu(
      QMenu&,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager&) override;

Q_SIGNALS:
  void pressed(QPointF);
  void moved(QPointF);
  void released(QPointF);
  void escPressed();

private:
  void updateAllElements();
  void on_intervalExecutionTimer();

  const Loop::ProcessModel& m_layer;
  graphics_item_ptr<LayerView> m_view;

  ZoomRatio m_zoomRatio{};

  ViewUpdater m_viewUpdater;

  ToolPalette m_palette;
};
}
