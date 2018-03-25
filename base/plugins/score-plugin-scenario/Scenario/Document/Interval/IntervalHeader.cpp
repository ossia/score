// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalHeader.hpp"
#include "IntervalView.hpp"

#include <QCursor>

class QGraphicsSceneMouseEvent;
namespace Scenario
{
static const QCursor& openCursor() {
  static const QCursor c{Qt::OpenHandCursor};
  return c;
}
static const QCursor& closedCursor() {
  static const QCursor c{Qt::ClosedHandCursor};
  return c;
}
void IntervalHeader::setWidth(double width)
{
  prepareGeometryChange();
  m_width = width;
  if(this->cursor().shape() != openCursor().shape())
    this->setCursor(openCursor());
  update();
}

void IntervalHeader::setText(const QString& text)
{
  m_text = text;
  on_textChange();
  update();
}

void IntervalHeader::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->cursor().shape() != closedCursor().shape())
    this->setCursor(closedCursor());
  m_view->mousePressEvent(event);
}

void IntervalHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_view->mouseMoveEvent(event);
}

void IntervalHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->cursor().shape() != openCursor().shape())
    this->setCursor(openCursor());
  m_view->mouseReleaseEvent(event);
}
}
