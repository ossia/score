#include "ConstraintHeader.hpp"
#include "ConstraintView.hpp"

class QGraphicsSceneMouseEvent;
namespace Scenario
{
void ConstraintHeader::setWidth(double width)
{
  m_width = width;
  update();
}

void ConstraintHeader::setText(const QString& text)
{
  m_text = text;
  on_textChange();
  update();
}

void ConstraintHeader::mousePressEvent(QMouseEvent* event)
{
  m_view->mousePressEvent(event);
}

void ConstraintHeader::mouseMoveEvent(QMouseEvent* event)
{
  m_view->mouseMoveEvent(event);
}

void ConstraintHeader::mouseReleaseEvent(QMouseEvent* event)
{
  m_view->mouseReleaseEvent(event);
}
}
