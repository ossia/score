#include "ConstraintHeader.hpp"
#include "ConstraintView.hpp"

#include <QCursor>

class QGraphicsSceneMouseEvent;
namespace Scenario
{
void ConstraintHeader::setWidth(double width)
{
  prepareGeometryChange();
  m_width = width;
  this->setCursor(QCursor(Qt::OpenHandCursor));
}

void ConstraintHeader::setText(const QString& text)
{
  m_text = text;
  on_textChange();
  update();
}

void ConstraintHeader::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  this->setCursor(QCursor(Qt::ClosedHandCursor));
  m_view->mousePressEvent(event);
}

void ConstraintHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_view->mouseMoveEvent(event);
}

void ConstraintHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  this->setCursor(QCursor(Qt::OpenHandCursor));
  m_view->mouseReleaseEvent(event);
}
}
