// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Minimap/Minimap.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QWidget>
#include <Process/Style/ScenarioStyle.hpp>
#include <ossia/detail/math.hpp>
#include <QGraphicsView>
#include <QApplication>
namespace Scenario
{
Minimap::Minimap(QGraphicsView* vp):
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
  visibleRectChanged(m_leftHandle, m_rightHandle);
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
  painter->setPen(sk.MinimapPen);
  painter->setBrush(sk.MinimapBrush);
  painter->drawRoundedRect(QRectF{m_leftHandle, 2., m_rightHandle - m_leftHandle, m_height - 3.}, 4, 4);
  painter->drawRoundedRect(QRectF{m_leftHandle, 2., m_rightHandle - m_leftHandle, m_height - 3.}, 4, 4);
}

#if defined(__APPLE__)
std::chrono::time_point<std::chrono::high_resolution_clock> t0, t1;
#endif
void Minimap::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
#if defined(__APPLE__)
    t0 = std::chrono::high_resolution_clock::now();
    t1 = std::chrono::high_resolution_clock::now();
#endif
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
  m_relativeStartX = (ev->pos().x() - m_leftHandle) / (m_rightHandle - m_leftHandle);
  m_startY = ev->pos().y();

  if(m_setCursor)
  {
    QApplication::changeOverrideCursor(QCursor(Qt::BlankCursor));
  }
  else
  {
    QApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    m_setCursor = true;
  }
  ev->accept();
}
void Minimap::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
#if defined(__APPLE__)
    t1 = std::chrono::high_resolution_clock::now();
    if(t1 - t0 < std::chrono::milliseconds(16))
    {
        return;
    }
    t0 = t1;
#endif
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

      if(!((m_leftHandle <= 0. && dx < 0) || (m_rightHandle >= m_width && dx > 0)))
      {
        setHandles(
              m_leftHandle  + dx - dy,
              m_rightHandle + dx + dy);
      }
    }

    QCursor::setPos(m_startPos);

    visibleRectChanged(m_leftHandle, m_rightHandle);

    ev->accept();
    return;
  }
  ev->ignore();
}

void Minimap::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  if(m_setCursor)
  {
    QApplication::restoreOverrideCursor();
    m_setCursor = false;
  }

  if(m_gripLeft || m_gripRight || m_gripMid)
  {
    m_gripLeft = false;
    m_gripRight = false;
    m_gripMid = false;

    QPointF pos;
    pos.setX(ossia::clamp(
               m_leftHandle + m_relativeStartX * (m_rightHandle - m_leftHandle),
               m_leftHandle,
               m_rightHandle));
    pos.setY(m_startY);

    QCursor::setPos(m_viewport->mapToGlobal(pos.toPoint()));
    ev->accept();
    return;
  }
  ev->ignore();
}

void Minimap::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
  rescale();
  ev->accept();
}

void Minimap::hoverEnterEvent(QGraphicsSceneHoverEvent* ev)
{
  const auto pos_x = ev->pos().x();
  if(std::abs(pos_x - m_leftHandle) < 3.)
  {
    if(!m_setCursor)
    {
      QApplication::setOverrideCursor(Qt::SizeHorCursor);
      m_setCursor = true;
    }
  }
  else if(std::abs(pos_x - m_rightHandle) < 3.)
  {
    if(!m_setCursor)
    {
      QApplication::setOverrideCursor(Qt::SizeHorCursor);
      m_setCursor = true;
    }
  }
  else if(pos_x > m_leftHandle && pos_x < m_rightHandle)
  {
    if(!m_setCursor)
    {
      QApplication::setOverrideCursor(Qt::SizeAllCursor);
      m_setCursor = true;
    }
  }
  else
  {
    if(m_setCursor)
    {
      QApplication::restoreOverrideCursor();
      m_setCursor = false;
    }
  }
}
void Minimap::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
  this->hoverEnterEvent(ev);
}

void Minimap::hoverLeaveEvent(QGraphicsSceneHoverEvent* e)
{
  if(m_setCursor)
  {
    QApplication::restoreOverrideCursor();
    m_setCursor = false;
  }
}

}

