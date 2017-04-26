#include "ConstraintBrace.hpp"
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Constraint/ConstraintPresenter.hpp>

using namespace Scenario;

ConstraintBrace::ConstraintBrace(
    const ConstraintView& parentCstr, QGraphicsItem* parent)
    : QGraphicsItem(), m_parent{parentCstr}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setCursor(Qt::SizeHorCursor);
  this->setZValue(ZPos::Brace);

  m_path.moveTo(10, -10);
  m_path.arcTo(0, -10, 20, 20, 90, 180);

  this->setParentItem(parent);
}

QRectF ConstraintBrace::boundingRect() const
{
  return {0, -10, 10, 20};
}

void ConstraintBrace::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(skin.DefaultBrush);

  if (m_parent.isSelected())
  {
    painter->setPen(skin.ConstraintBraceSelected);
  }
  else if (m_parent.warning())
  {
    painter->setPen(skin.ConstraintBraceWarning);
  }
  else if (!m_parent.isValid())
  {
    painter->setPen(skin.ConstraintBraceInvalid);
  }
  else
  {
    painter->setPen(skin.ConstraintBrace);
  }

  painter->drawPath(m_path);

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::lightGray);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(boundingRect());
#endif
}

void ConstraintBrace::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    m_parent.presenter().pressed(event->scenePos());
}

void ConstraintBrace::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_parent.presenter().moved(event->scenePos());
}

void ConstraintBrace::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_parent.presenter().released(event->scenePos());
}
