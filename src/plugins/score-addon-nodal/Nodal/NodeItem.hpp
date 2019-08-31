#pragma once
#include <QGraphicsItem>
#include <Process/ZoomHelper.hpp>
#include <score/model/Identifier.hpp>
#include <Effect/EffectPainting.hpp>
namespace score
{
struct DocumentContext;
class SimpleTextItem;
}
namespace Dataflow {
class PortItem;
}
namespace Process
{
class ProcessModel;
class LayerPresenter;
struct LayerContext;
}

namespace Nodal
{
class TitleItem;
class Node;
class NodeItem : public Effect::ItemBase
{
public:
  NodeItem(const Node& model, const Process::Context& ctx, QGraphicsItem* parent);
  const Id<Node>& id() const noexcept;
  ~NodeItem();

  void setZoomRatio(ZoomRatio r);
  void setPlayPercentage(float f);

  qreal width() const noexcept { return m_size.width(); }

private:
  void updateSize();
  void setSize(QSizeF sz);

  bool isInSelectionCorner(QPointF f, QRectF r) const;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  void resetInlets(Process::ProcessModel& effect);
  void resetOutlets(Process::ProcessModel& effect);

  const Node& m_model;

  //Body
  QGraphicsItem* m_fx{};
  Process::LayerPresenter* m_presenter{};

  std::vector<Dataflow::PortItem*> m_inlets, m_outlets;
  const Process::Context& m_context;

  ZoomRatio m_ratio{1.};
  double m_playPercentage{};
};
}
