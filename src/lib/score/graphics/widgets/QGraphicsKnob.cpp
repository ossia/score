#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsKnob);

namespace score
{

QGraphicsKnob::QGraphicsKnob(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsKnob::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsKnob::setRange(double min, double max)
{
  this->min = min;
  this->max = max;
  update();
}

void QGraphicsKnob::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

void QGraphicsKnob::setExecutionValue(double v)
{
  m_execValue = ossia::clamp(v, 0., 1.);
  m_hasExec = true;
  update();
}

void QGraphicsKnob::resetExecution()
{
  m_hasExec = false;
  update();
}

double QGraphicsKnob::value() const
{
  return m_value;
}

void QGraphicsKnob::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsKnob::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsKnob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsKnob::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsKnob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsKnob::boundingRect() const
{
  return m_rect;
}

void QGraphicsKnob::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  const double val = map(m_value);
  const double abs = std::abs(val);
  int pres = abs < 10. ? 3 : abs < 100. ? 2 : abs < 1000. ? 1 : 0;
  DefaultGraphicsKnobImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(val, 'f', pres),
      painter,
      widget);
}
}
