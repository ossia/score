#pragma once
#include <Media/ChainProcess.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <nano_observer.hpp>

namespace Media
{
class EffectItem;
}
namespace Media
{

class View final : public Process::LayerView
{
public:
  explicit View(QGraphicsItem* parent);

  void setup(const Media::ChainProcess& object, const Process::LayerContext& ctx);

  int findDropPosition(QPointF pos) const;
  void setInvalid(bool b);

  const std::vector<std::shared_ptr<EffectItem>>& effects() const;

private:
  friend class EffectItem;
  void recomputeItemPositions() const;
  void paint_impl(QPainter* p) const override;
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* ev) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* ev) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* ev) override;
  void dropEvent(QGraphicsSceneDragDropEvent* ev) override;

  std::vector<std::shared_ptr<EffectItem>> m_effects;
  bool m_invalid{};
};

class Presenter final
    : public Process::LayerPresenter
    , public Nano::Observer
{
public:
  explicit Presenter(
      const Media::ChainProcess& model,
      View* view,
      const Process::Context& ctx,
      QObject* parent);

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal val) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;
  void parentGeometryChanged() override;

  void on_drop(const QMimeData& mime, int pos);
  void on_effectAdded(const Process::ProcessModel& p);
  void on_effectRemoved(const Process::ProcessModel& p);
  void on_orderChanged();

private:
  View* m_view{};
};
}
