#include "ConstraintBrace.hpp"
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>

using namespace Scenario;

ConstraintBrace::ConstraintBrace(
    const ConstraintView& parentCstr, QQuickPaintedItem* parent)
    : GraphicsItem(parent), m_parent{parentCstr}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setCursor(Qt::SizeHorCursor);
  this->setZ(ZPos::Brace);

  this->setWidth(10);
  this->setHeight(20);
  m_path.moveTo(10, -10);
  m_path.arcTo(0, -10, 20, 20, 90, 180);
}

QRectF ConstraintBrace::clipRect() const
{
  return {0, -10, 10, 20};
}

void ConstraintBrace::paint(
    QPainter* painter)
{
  painter->setBrush({});
  painter->setRenderHint(QPainter::Antialiasing, true);
  QPen pen{{}, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
  // TODO make a switch instead and transform these to use Q_FLAGS or something
  if (m_parent.isSelected())
  {
    pen.setBrush(ScenarioStyle::instance().ConstraintSelected.getColor());
  }
  else if (m_parent.warning())
  {
    pen.setBrush(ScenarioStyle::instance().ConstraintWarning.getColor());
  }
  else
  {
    pen.setBrush(ScenarioStyle::instance().ConstraintBase.getColor());
  }

  if (!m_parent.isValid())
  {
    pen.setBrush(ScenarioStyle::instance().ConstraintInvalid.getColor());
  }

  painter->setPen(pen);

  painter->drawPath(m_path);

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::lightGray);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(boundingRect());
#endif
}

void ConstraintBrace::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    m_parent.presenter().pressed(mapToScene(event->localPos()));
}

void ConstraintBrace::mouseMoveEvent(QMouseEvent* event)
{
  m_parent.presenter().moved(mapToScene(event->localPos()));
}

void ConstraintBrace::mouseReleaseEvent(QMouseEvent* event)
{
  m_parent.presenter().released(mapToScene(event->localPos()));
}
