#pragma once
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <QGraphicsItem>

#include <wobjectdefs.h>

namespace Scenario
{
class IntervalPresenter;
class SlotHeader final : public QGraphicsItem
{
public:
  SlotHeader(
      const IntervalPresenter& slotView,
      int slotIndex,
      QGraphicsItem* parent);

  const IntervalPresenter& presenter() const { return m_presenter; }
  static constexpr int static_type()
  {
    return QGraphicsItem::UserType + ItemType::SlotHeader;
  }
  int type() const override { return static_type(); }

  int slotIndex() const;
  void setSlotIndex(int);
  static constexpr double headerHeight() { return 16.; }
  static constexpr double handleWidth() { return 16.; }
  static constexpr double menuWidth() { return 16.; }

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setWidth(qreal width);
  void setMini(bool b);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  const IntervalPresenter& m_presenter;
  qreal m_width{};
  // double m_menupos{};
  int m_slotIndex{};
};

class SlotDragOverlay final : public QObject, public QGraphicsItem
{
  W_OBJECT(SlotDragOverlay)
public:
  SlotDragOverlay(const IntervalPresenter& c, Slot::RackView v);

  const IntervalPresenter& interval;
  Slot::RackView view;
  QRectF boundingRect() const override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void dropBefore(int slot) W_SIGNAL(dropBefore, slot);
  void dropIn(int slot) W_SIGNAL(dropIn, slot);

  void onDrag(QPointF pos);

private:
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;

  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  QRectF m_drawnRect;
};
}
