#include <Scenario/Document/Minimap/Minimap.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QWidget>
#include <ossia/detail/math.hpp>
namespace Scenario
{

Minimap::Minimap(QWidget* vp):
  m_viewport{vp}
{
  m_leftHandle = 100;
  m_rightHandle = 200;
}

void Minimap::setVisibleDuration(TimeVal t)
{

}

void Minimap::setWidth(double d)
{
  m_width = d;
}

void Minimap::setLeftHandleTime(TimeVal t)
{

}

void Minimap::setRightHandleTime(TimeVal t)
{

}

QRectF Minimap::boundingRect() const
{
  return {0, 0, m_width, m_height};
}

void Minimap::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setPen(Qt::cyan);
  painter->drawLine(QPointF{m_leftHandle, 0}, QPointF{m_leftHandle, m_height});
  painter->drawLine(QPointF{m_rightHandle, 0}, QPointF{m_rightHandle, m_height});

}

void Minimap::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  m_gripLeft = false;
  m_gripRight = false;
  if(std::abs(ev->pos().x() - m_leftHandle) < 3.)
    m_gripLeft = true;
  else if(std::abs(ev->pos().x() - m_rightHandle) < 3.)
    m_gripRight = true;

  ev->accept();
}

void Minimap::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  if(m_gripLeft)
  {
    m_leftHandle = ossia::clamp(ev->pos().x(), 0., m_rightHandle - 5);
    update();
    m_viewport->update();
  }
  else if(m_gripRight)
  {
    m_rightHandle = ossia::clamp(ev->pos().x(), m_leftHandle + 5, m_width);
    update();
    m_viewport->update();
  }
  emit visibleRectChanged({m_leftHandle, 0, m_rightHandle - m_leftHandle, 0});
  ev->accept();
}

void Minimap::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  m_gripLeft = false;
  m_gripRight = false;
  update();
  ev->accept();
}

}

