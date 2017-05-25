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
}

void Minimap::setWidth(double d)
{
  prepareGeometryChange();
  m_width = d;
  update();
  m_viewport->update();
}

void Minimap::setLeftHandle(double d)
{
  m_leftHandle = d;
  update();
  m_viewport->update();
}

void Minimap::setRightHandle(double d)
{
  m_rightHandle = d;
  update();
  m_viewport->update();
}

QRectF Minimap::boundingRect() const
{
  return {0, 0, m_width, m_height};
}

void Minimap::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setPen(QPen(QColor(0, 255, 0, 255), 2));
  painter->drawRect(QRectF{m_leftHandle, 2., m_rightHandle - m_leftHandle, m_height - 3.});
}

void Minimap::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  m_gripLeft = false;
  m_gripRight = false;
  m_gripMid = false;
  if(std::abs(ev->pos().x() - m_leftHandle) < 3.)
    m_gripLeft = true;
  else if(std::abs(ev->pos().x() - m_rightHandle) < 3.)
    m_gripRight = true;
  else if(ev->pos().x() > m_leftHandle && ev->pos().x() < m_rightHandle)
    m_gripMid = true;

  ev->accept();
}

void Minimap::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  if(m_gripLeft || m_gripRight || m_gripMid)
  {
    if(m_gripLeft)
    {
      m_leftHandle = ossia::clamp(ev->pos().x(), 0., m_rightHandle - 5);
    }
    else if(m_gripRight)
    {
      m_rightHandle = ossia::clamp(ev->pos().x(), m_leftHandle + 5, m_width);
    }
    else if(m_gripMid)
    {
      auto orig = ev->lastPos().x();
      auto dx = ev->pos().x() - orig;

      m_leftHandle += dx;
      m_rightHandle += dx;
      m_leftHandle = ossia::clamp(m_leftHandle, 0., m_rightHandle - 5);
      m_rightHandle = ossia::clamp(m_rightHandle, m_leftHandle + 5, m_width);
    }

    update();
    m_viewport->update();

    emit visibleRectChanged(m_leftHandle, m_rightHandle);
  }
  ev->accept();
}

void Minimap::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  m_gripLeft = false;
  m_gripRight = false;
  m_gripMid = false;
  ev->accept();
}

}

