#pragma once
#include <Process/LayerPresenter.hpp>

#include <Patternist/PatternModel.hpp>

#include <nano_observer.hpp>

namespace Patternist
{
class View;
class Controller;
class Presenter final
    : public Process::LayerPresenter
    , public Nano::Observer
{
public:
  explicit Presenter(
      const Patternist::ProcessModel& model, View* view, const Process::Context& ctx,
      QObject* parent);
  ~Presenter() override;

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

  void fillContextMenu(
      QMenu&, QPoint pos, QPointF scenepos,
      const Process::LayerContextMenuManager&) override;

private:
  void on_drop(const QPointF& pos, const QMimeData& md);

  View* m_view{};
  Controller* m_controller{};
};
}
