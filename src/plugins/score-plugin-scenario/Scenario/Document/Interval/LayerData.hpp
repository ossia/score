#pragma once
#include <Process/ZoomHelper.hpp>
#include <QPoint>
#include <QPointF>
#include <score/graphics/RectItem.hpp>
#include <vector>

class QPixmap;
class QMenu;
class QGraphicsItem;
class QObject;
namespace Process
{
struct ProcessPresenterContext;
class ProcessModel;
class LayerPresenter;
class LayerView;
class LayerFactory;
class LayerContextMenuManager;
class GraphicsShapeItem;
}

namespace Scenario
{
class LayerRectItem
    : public score::ResizeableItem
{
public:
  LayerRectItem(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  QRectF rect() const noexcept;

private:
  QRectF boundingRect() const;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget);

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
  void mousePressEvent(QGraphicsSceneMouseEvent* event);
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

private:
  QRectF m_rect{};
};


class LayerData
{
public:
  struct Layer
  {
    Process::LayerPresenter* presenter{};
    Scenario::LayerRectItem* container{};
    Process::LayerView* view{};
  };
  LayerData() = default;
  LayerData(const LayerData&) = delete;
  LayerData(LayerData&&) = default;
  LayerData& operator=(const LayerData&) = default;
  LayerData& operator=(LayerData&&) = default;
  LayerData(const Process::ProcessModel* m);

  Process::LayerPresenter* mainPresenter() const noexcept;
  Process::LayerView* mainView() const noexcept;

  void cleanup();
  void addView(
      Process::LayerFactory& factory,
      ZoomRatio zoomRatio,
      const Process::ProcessPresenterContext& context,
      QGraphicsItem* parentItem,
      QObject* parent);
  void setupView(Layer& layer,
                 int idx,
                 qreal parentWidth, qreal parent_default_width,
                 qreal w, qreal h);

  std::size_t count() const noexcept { return m_layers.size(); }
  void removeView(int i);

  // Presenter API
  bool focused() const;
  void setFocus(bool focus) const;
  void on_focusChanged() const;

  void setFullView() const;

  void setWidth(qreal width, qreal defaultWidth) const;
  void setHeight(qreal height) const;

  void putToFront() const;
  void putBehind() const;

  void on_zoomRatioChanged(
      const Process::ProcessPresenterContext& lst,
      ZoomRatio r,
      qreal parentWidth, qreal parent_default_width,
      qreal slot_height,
      QGraphicsItem* parentItem,
      QObject* parent);

  void updateLoops(
      const Process::ProcessPresenterContext& lst,
      ZoomRatio r,
      qreal parentWidth, qreal parent_default_width,
      qreal slot_height,
      QGraphicsItem* parentItem,
      QObject* parent);

  void parentGeometryChanged() const;

  void fillContextMenu(
      QMenu& m,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager& mgr) const;

  Process::GraphicsShapeItem* makeSlotHeaderDelegate() const;

  // View API
  void updatePositions(qreal y, qreal instancewidth);

  void updateXPositions(qreal instancewidth) const;

  void updateYPositions(qreal y);

  void updateContainerWidths(qreal w) const;

  void updateContainerHeights(qreal h) const;

  void updateStartOffset(double x) const;

  void update() const;

  void setZValue(qreal z) const;

  QPixmap pixmap() const noexcept;

  const Process::ProcessModel& model() const noexcept { return *m_model; }

  const std::vector<Layer>& layers() const noexcept { return m_layers; }
private:
  const Process::ProcessModel* m_model{};

  std::vector<Layer> m_layers;
  qreal m_slotY{};
};

}
