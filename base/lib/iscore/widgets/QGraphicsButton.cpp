#include <iscore/widgets/QGraphicsButton.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

QGraphicsButton::QGraphicsButton(QImage clicked, QImage released):
  m_clicked{clicked}
, m_released{released}
{

}

QRectF QGraphicsButton::boundingRect() const
{
  return QRectF(m_clicked.rect());
}

void QGraphicsButton::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  if(m_click)
    painter->drawImage(0, 0, m_clicked);
  else
    painter->drawImage(0, 0, m_released);
}

void QGraphicsButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_click = true;
  event->accept();
  emit clicked();
}

void QGraphicsButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_click = false;
  event->accept();
}
