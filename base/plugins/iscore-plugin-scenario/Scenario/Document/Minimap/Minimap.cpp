#include <Scenario/Document/Minimap/Minimap.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QWidget>
#include <Process/Style/ScenarioStyle.hpp>
#include <ossia/detail/math.hpp>
#include <QApplication>
namespace Scenario
{
Minimap::Minimap(QWidget* vp):
  m_viewport{vp}
{
  this->setAcceptHoverEvents(true);
}

void Minimap::setWidth(double d)
{
  prepareGeometryChange();
  m_width = d;
  update();
}

void Minimap::setMinDistance(double d)
{
  m_minDist = d;
}

void Minimap::setLeftHandle(double l)
{
  m_leftHandle = ossia::clamp(l, 0., m_rightHandle - m_minDist);
  update();
}

void Minimap::setRightHandle(double r)
{
  m_rightHandle = ossia::clamp(r, m_leftHandle + m_minDist, m_width);
  update();
}

void Minimap::setHandles(double l, double r)
{
  m_leftHandle = ossia::clamp(l, 0., m_rightHandle - m_minDist);
  m_rightHandle = ossia::clamp(r, m_leftHandle + m_minDist, m_width);
  update();
}

void Minimap::modifyHandles(double l, double r)
{
  setHandles(l, r);
  emit visibleRectChanged(m_leftHandle, m_rightHandle);
}

void Minimap::setLargeView()
{
  modifyHandles(0., m_width);
}

void Minimap::zoomIn()
{
  modifyHandles(
        m_leftHandle + 0.01 * (m_rightHandle - m_leftHandle),
        m_rightHandle - 0.01 * (m_rightHandle - m_leftHandle));
}

void Minimap::zoomOut()
{
  modifyHandles(
        m_leftHandle - 0.01 * (m_rightHandle - m_leftHandle),
        m_rightHandle + 0.01 * (m_rightHandle - m_leftHandle));
}

void Minimap::zoom(double z)
{
  modifyHandles(m_leftHandle + z, m_rightHandle - z);
}

QRectF Minimap::boundingRect() const
{
  return {0., 0., m_width, m_height};
}

void Minimap::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& sk = ScenarioStyle::instance();
  painter->setPen(sk.TimenodePen);
  painter->drawRect(QRectF{m_leftHandle, 2., m_rightHandle - m_leftHandle, m_height - 3.});
}

void Minimap::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  m_gripLeft = false;
  m_gripRight = false;
  m_gripMid = false;

  const auto pos_x = ev->pos().x();

  if(std::abs(pos_x - m_leftHandle) < 3.)
    m_gripLeft = true;
  else if(std::abs(pos_x - m_rightHandle) < 3.)
    m_gripRight = true;
  else if(pos_x > m_leftHandle && pos_x < m_rightHandle)
    m_gripMid = true;
  else
  {
    ev->ignore();
    return;
  }

  m_startPos = ev->screenPos();
  m_lastPos = m_startPos;

  QApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
  ev->accept();
}

void Minimap::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  const auto pos = ev->screenPos();
  if(m_gripLeft || m_gripRight || m_gripMid)
  {
    auto dx = 0.7 * (pos.x() - m_startPos.x());
    if(m_gripLeft)
    {
      setLeftHandle(m_leftHandle + dx);
    }
    else if(m_gripRight)
    {
      setRightHandle(m_rightHandle + dx);
    }
    else if(m_gripMid)
    {
      auto dy = 0.7 * (pos.y() - m_startPos.y());

      setHandles(
            m_leftHandle  + dx - dy,
            m_rightHandle + dx + dy);
    }

    QCursor::setPos(m_startPos);
    emit visibleRectChanged(m_leftHandle, m_rightHandle);
  }
  ev->accept();
}

void Minimap::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  m_gripLeft = false;
  m_gripRight = false;
  m_gripMid = false;
  QCursor::setPos(m_startPos);
  QApplication::restoreOverrideCursor();
  ev->accept();
}

void Minimap::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
  emit rescale();
  ev->accept();
}

void Minimap::hoverEnterEvent(QGraphicsSceneHoverEvent* ev)
{
  const auto pos_x = ev->pos().x();
  if(std::abs(pos_x - m_leftHandle) < 3.)
  {
    QApplication::setOverrideCursor(Qt::SizeHorCursor);
  }
  else if(std::abs(pos_x - m_rightHandle) < 3.)
  {
    QApplication::setOverrideCursor(Qt::SizeHorCursor);
  }
  else if(pos_x > m_leftHandle && pos_x < m_rightHandle)
  {
    QApplication::setOverrideCursor(Qt::SizeAllCursor);
  }
  else
  {
    QApplication::restoreOverrideCursor();
  }
}
void Minimap::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
  this->hoverEnterEvent(ev);
}

void Minimap::hoverLeaveEvent(QGraphicsSceneHoverEvent* e)
{
  QApplication::restoreOverrideCursor();
}

}

