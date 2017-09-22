// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalHeader.hpp"
#include "IntervalView.hpp"

#include <QCursor>

class QGraphicsSceneMouseEvent;
namespace Scenario
{
void IntervalHeader::setWidth(double width)
{
  prepareGeometryChange();
  m_width = width;
  this->setCursor(QCursor(Qt::OpenHandCursor));
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
  this->setCursor(QCursor(Qt::ClosedHandCursor));
  m_view->mousePressEvent(event);
}

void IntervalHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_view->mouseMoveEvent(event);
}

void IntervalHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  this->setCursor(QCursor(Qt::OpenHandCursor));
  m_view->mouseReleaseEvent(event);
}
}
