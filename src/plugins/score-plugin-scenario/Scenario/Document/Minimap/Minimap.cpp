// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>

#include <Scenario/Document/Minimap/Minimap.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/graphics/InfiniteScroller.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QWidget>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::Minimap)
namespace Scenario
{
Minimap::Minimap()
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

void Minimap::restoreHandles(double l, double r)
{
  m_leftHandle = std::max(0., l);
  m_rightHandle = std::min(r, m_width);
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

void Minimap::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& sk = Process::Style::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->fillRect(
      QRectF{m_leftHandle, 0., m_rightHandle - m_leftHandle, m_height},
      sk.MinimapBrush());

  painter->setPen(sk.MinimapPen());
  const double line_length = 4;
  const QPointF top_left{m_leftHandle, 0};
  painter->drawLine(top_left, top_left + QPointF{line_length, 0.});
  painter->drawLine(top_left, top_left + QPointF{0., line_length});

  const QPointF top_right{m_rightHandle - 1., 0};
  painter->drawLine(top_right, top_right + QPointF{-line_length, 0.});
  painter->drawLine(top_right, top_right + QPointF{0., line_length});

  const QPointF bottom_left{m_leftHandle, m_height - 0.5};
  painter->drawLine(bottom_left, bottom_left + QPointF{line_length, 0.});
  painter->drawLine(bottom_left, bottom_left + QPointF{0., -line_length});

  const QPointF bottom_right{m_rightHandle - 1., m_height - 0.5};
  painter->drawLine(bottom_right, bottom_right + QPointF{-line_length, 0.});
  painter->drawLine(bottom_right, bottom_right + QPointF{0., -line_length});
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

  score::InfiniteScroller::start(*this, 0.);
  ev->accept();
}
void Minimap::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  if(m_gripLeft || m_gripRight || m_gripMid)
  {
    // Relative, so that the handles keep travelling once the pointer would
    // have run out of screen -- which is the whole point of holding it still.
    const auto delta = score::InfiniteScroller::relativeMotion(ev);

    // Not at the press: the cursor is only worth blanking once something is
    // actually keeping it on the handle. Changing the override the hover
    // pushed, rather than pushing another, keeps the stack depth where
    // m_setCursor says it is.
    if(!m_hidCursor && m_setCursor && score::InfiniteScroller::holdsPointer())
    {
      QApplication::changeOverrideCursor(QCursor(Qt::BlankCursor));
      m_hidCursor = true;
    }

    if(m_gripLeft)
    {
      setLeftHandle(m_leftHandle + delta.x());
    }
    else if(m_gripRight)
    {
      setRightHandle(m_rightHandle + delta.x());
    }
    else if(m_gripMid)
    {
      const auto dx = delta.x();
      const auto dy = delta.y();

      auto newLeftHandle = std::max(m_leftHandle + dx - dy, 0.);
      auto newRightHandle = std::min(m_rightHandle + dx + dy, m_width);
      if(m_leftHandle <= 5. && newLeftHandle <= 5.)
        newRightHandle = m_rightHandle;
      if(m_rightHandle >= m_width - 5. && newRightHandle >= m_width - 5.)
        newLeftHandle = m_leftHandle;

      if(newLeftHandle != m_leftHandle || newRightHandle != m_rightHandle)
      {
        setHandles(newLeftHandle, newRightHandle);
      }
    }

    visibleRectChanged(m_leftHandle, m_rightHandle);
  }
  ev->accept();
}

void Minimap::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  if(m_gripLeft || m_gripRight || m_gripMid)
  {
    score::InfiniteScroller::stop(*this, ev);

    // Put back the shape the hover had chosen, again without touching depth.
    if(m_hidCursor)
    {
      QApplication::changeOverrideCursor(cursorFor(ev->pos().x()));
      m_hidCursor = false;
    }
  }

  m_gripLeft = false;
  m_gripRight = false;
  m_gripMid = false;

  ev->accept();
}

void Minimap::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
  rescale();
  ev->accept();
}

QCursor Minimap::cursorFor(double pos_x) const
{
  auto& skin = score::Skin::instance();
  if(std::abs(pos_x - m_leftHandle) < 3. || std::abs(pos_x - m_rightHandle) < 3.)
    return skin.CursorScaleH;
  if(pos_x > m_leftHandle && pos_x < m_rightHandle)
    return skin.CursorMagnifier;
  return QCursor{};
}

void Minimap::hoverEnterEvent(QGraphicsSceneHoverEvent* ev)
{
  auto& skin = score::Skin::instance();

  const auto pos_x = ev->pos().x();
  if(std::abs(pos_x - m_leftHandle) < 3.)
  {
    if(!m_setCursor)
    {
      QApplication::setOverrideCursor(skin.CursorScaleH);
      m_setCursor = true;
    }
  }
  else if(std::abs(pos_x - m_rightHandle) < 3.)
  {
    if(!m_setCursor)
    {
      QApplication::setOverrideCursor(skin.CursorScaleH);
      m_setCursor = true;
    }
  }
  else if(pos_x > m_leftHandle && pos_x < m_rightHandle)
  {
    if(!m_setCursor)
    {
      QApplication::setOverrideCursor(skin.CursorMagnifier);
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
