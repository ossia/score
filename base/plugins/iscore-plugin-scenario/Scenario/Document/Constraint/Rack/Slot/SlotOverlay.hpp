#pragma once
#include <QQuickPaintedItem>
#include <QRect>
#include <QtGlobal>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

class QGraphicsSceneMouseEvent;
class QPainter;

class QWidget;

namespace Scenario
{
class SlotView;
class SlotOverlay final : public QQuickPaintedItem
{
public:
  SlotOverlay(SlotView* parent);

  static constexpr int static_type()
  {
    return 1337 + ItemType::SlotOverlay;
  }
  int type() const override
  {
    return static_type();
  }

  const SlotView& slotView() const
  {
    return m_slotView;
  }

  QRectF boundingRect() const override;

  void setHeight(qreal height);
  void setWidth(qreal height);

  virtual void paint(
      QPainter* painter) override;

  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  const SlotView& m_slotView;
};
}
