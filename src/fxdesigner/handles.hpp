#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <verdigris>

namespace fxd
{
class MoveHandle
        : public QObject
        , public QGraphicsItem
{
  W_OBJECT(MoveHandle)
public:
  MoveHandle()
  {
    auto& skin = score::Skin::instance();
    setCursor(skin.CursorMove);
    setAcceptHoverEvents(true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemSendsScenePositionChanges, true);
  }
  void setRect(QRectF r)
  {
    prepareGeometryChange();
    m_rect = r;
    update();
  }

  QRectF boundingRect() const override
  {
    return m_rect;
  }
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    if(m_hover)
    {
      painter->setPen(QPen(Qt::gray));
      painter->setBrush(QBrush(QColor(255, 255, 255, 30)));
      painter->drawRect(m_rect);
    }
    else if(isSelected())
    {
      painter->setPen(QPen(Qt::gray, 1, Qt::PenStyle::DotLine));
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(m_rect);
    }
  }

  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
  {
    switch(change)
    {
      case GraphicsItemChange::ItemScenePositionHasChanged:
        positionChanged(value.toPointF());
        break;
      case GraphicsItemChange::ItemSelectedHasChanged:
        selectionChanged(value.toBool());
        break;
      default:
        break;
    }
    return QGraphicsItem::itemChange(change, value);
  }

  void released() W_SIGNAL(released)
  void positionChanged(QPointF p) W_SIGNAL(positionChanged, p)
  void selectionChanged(bool b) W_SIGNAL(selectionChanged, b)

  private:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
    {
      m_hover = true;
      event->accept();
      update();
    }
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
    {
      event->accept();
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
    {
      m_hover = false;
      event->accept();
      update();
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
      QGraphicsItem::mouseReleaseEvent(event);
      released();
    }

    QRectF m_rect{};
    bool m_hover{};


};
class ResizeHandle
        : public QObject
        , public QGraphicsItem
{
  W_OBJECT(ResizeHandle)
public:
  ResizeHandle()
  {
    auto& skin = score::Skin::instance();
    setCursor(skin.CursorScaleFDiag);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemSendsScenePositionChanges, true);
  }
  QRectF boundingRect() const override
  {
    return {0., 0., 12., 12.};
  }
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    painter->setPen(Qt::white);
    //painter->drawLine(QPointF{11., 0.}, QPointF{12., 12.});
    //painter->drawLine(QPointF{0., 11.}, QPointF{12., 12.});
    painter->drawRect(boundingRect());
  }

  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
  {
    switch(change)
    {
      case GraphicsItemChange::ItemScenePositionHasChanged:
        positionChanged(value.toPointF());
        break;
      default:
        break;
    }
    return QGraphicsItem::itemChange(change, value);
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override
  {
    QGraphicsItem::mouseReleaseEvent(ev);
    released();
  }

  void positionChanged(QPointF p) W_SIGNAL(positionChanged, p)
  void released() W_SIGNAL(released)
};

}
