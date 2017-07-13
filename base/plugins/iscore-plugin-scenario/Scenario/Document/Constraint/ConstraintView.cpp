// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QGraphicsSceneEvent>
#include <QtGlobal>
#include <QCursor>

#include "ConstraintPresenter.hpp"
#include "ConstraintView.hpp"
#include "ConstraintMenuOverlay.hpp"

namespace Scenario
{
ConstraintView::ConstraintView(
    ConstraintPresenter& presenter, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_leftBrace{*this, this}
    , m_rightBrace{*this, this}
    , m_labelItem{this}
    , m_counterItem{this}
    , m_presenter{presenter}
{
  setAcceptHoverEvents(true);
  m_leftBrace.setX(minWidth());
  m_leftBrace.hide();

  m_rightBrace.setX(maxWidth());
  m_rightBrace.hide();

  m_labelItem.setFont(ScenarioStyle::instance().Medium12Pt);
  m_labelItem.setPos(0, -16);
  m_labelItem.setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_labelItem.setAcceptHoverEvents(false);

  m_counterItem.setFont(ScenarioStyle::instance().Medium7Pt);
  m_counterItem.setColor(iscore::ColorRef(&iscore::Skin::Light));
  m_counterItem.setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_counterItem.setAcceptHoverEvents(false);
}

ConstraintView::~ConstraintView()
{
  delete m_overlay;
}

void ConstraintView::setInfinite(bool infinite)
{
  if(m_infinite != infinite)
  {
    prepareGeometryChange();

    m_infinite = infinite;
    updatePaths();
    update();
  }
}

void ConstraintView::setExecuting(bool e)
{
  m_waiting = e;
  update();
}

void ConstraintView::setDefaultWidth(double width)
{
  if(m_defaultWidth != width)
  {
    prepareGeometryChange();
    m_defaultWidth = width;
    updatePaths();
    update();
  }
}

void ConstraintView::setMaxWidth(bool infinite, double max)
{
  if(infinite != m_infinite || max != m_maxWidth)
  {
    prepareGeometryChange();

    setInfinite(infinite);
    if (!infinite)
    {
      m_maxWidth = max;
    }
    updatePaths();
    update();
  }
}

void ConstraintView::setMinWidth(double min)
{
  if(min != m_minWidth)
  {
    prepareGeometryChange();
    m_minWidth = min;
    updatePaths();
    update();
  }
}

void ConstraintView::setHeight(double height)
{
  if(m_height != height)
  {
    prepareGeometryChange();
    m_height = height;
    updatePaths();
    update();
  }
}

bool ConstraintView::setPlayWidth(double width)
{
  if(width != m_playWidth)
  {
    m_playWidth = width;
    updatePlayPaths();
    return true;
  }
  return false;
}

void ConstraintView::setValid(bool val)
{
  m_validConstraint = val;
}

void ConstraintView::setSelected(bool selected)
{
  m_selected = selected;
  setZValue(m_selected ? ZPos::SelectedConstraint : ZPos::Constraint);
  enableOverlay(selected);
  update();
}

void ConstraintView::setGripCursor()
{
  this->setCursor(QCursor(Qt::ClosedHandCursor));
}

void ConstraintView::setUngripCursor()
{
  this->setCursor(QCursor(Qt::OpenHandCursor));
}

void ConstraintView::enableOverlay(bool selected)
{

}

void ConstraintView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
  {
    if(event->pos().y() < 4)
      setGripCursor();
    else
      unsetCursor();
    emit m_presenter.pressed(event->scenePos());
  }
}

void ConstraintView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  emit m_presenter.moved(event->scenePos());
}

void ConstraintView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  emit m_presenter.released(event->scenePos());
  if(event->pos().y() < 4)
    setUngripCursor();
  else
    unsetCursor();
}

bool ConstraintView::warning() const
{
  return m_warning;
}

void ConstraintView::setWarning(bool warning)
{
  m_warning = warning;
}

const QBrush& ConstraintView::constraintColor(const ScenarioStyle& skin) const
{
  // TODO make a switch instead
  if (isSelected())
  {
    return skin.ConstraintSelected.getColor();
  }
  else if (warning())
  {
    return skin.ConstraintWarning.getColor();
  }
  else if (!isValid() || m_state == ConstraintExecutionState::Disabled)
  {
    return skin.ConstraintInvalid.getColor();
  }
  else if (m_state == ConstraintExecutionState::Muted)
  {
    return skin.ConstraintMuted.getColor();
  }
  else
  {
    return skin.ConstraintBase.getColor();
  }
}

void ConstraintView::updateOverlay()
{
  if(m_overlay) m_overlay->update();
  update();
}
void ConstraintView::updateLabelPos()
{
  m_labelItem.setPos(
        defaultWidth() / 2. - m_labelItem.boundingRect().width() / 2., -17);
}

void ConstraintView::updateCounterPos()
{
  m_counterItem.setPos(
        defaultWidth() - m_counterItem.boundingRect().width() - 5, 5);
}

void ConstraintView::updateOverlayPos()
{
  if(m_overlay)
    m_overlay->setPos(defaultWidth() / 2. - m_overlay->boundingRect().width() / 2, -10);
}

void ConstraintView::setExecutionState(ConstraintExecutionState s)
{
  m_state = s;
  update();
}
}
