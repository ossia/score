// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QGraphicsSceneEvent>
#include <QtGlobal>
#include <QCursor>

#include "IntervalPresenter.hpp"
#include "IntervalView.hpp"
#include "IntervalMenuOverlay.hpp"

namespace Scenario
{
IntervalView::IntervalView(
    IntervalPresenter& presenter, QGraphicsItem* parent)
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
  m_counterItem.setColor(score::ColorRef(&score::Skin::Light));
  m_counterItem.setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_counterItem.setAcceptHoverEvents(false);
}

IntervalView::~IntervalView()
{
  delete m_overlay;
}

void IntervalView::setInfinite(bool infinite)
{
  if(m_infinite != infinite)
  {
    prepareGeometryChange();

    m_infinite = infinite;
    updatePaths();
    update();
  }
}

void IntervalView::setExecuting(bool e)
{
  m_waiting = e;
  update();
}

void IntervalView::setDefaultWidth(double width)
{
  if(m_defaultWidth != width)
  {
    prepareGeometryChange();
    m_defaultWidth = width;
    updatePaths();
    update();
  }
}

void IntervalView::setMaxWidth(bool infinite, double max)
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

void IntervalView::setMinWidth(double min)
{
  if(min != m_minWidth)
  {
    prepareGeometryChange();
    m_minWidth = min;
    updatePaths();
    update();
  }
}

void IntervalView::setHeight(double height)
{
  if(m_height != height)
  {
    prepareGeometryChange();
    m_height = height;
    updatePaths();
    update();
  }
}

double IntervalView::setPlayWidth(double width)
{
  const auto v = std::abs(m_playWidth - width);
  if(v > 1.)
  {
    m_playWidth = width;
    updatePlayPaths();
    return v;
  }
  return 0.;
}

void IntervalView::setValid(bool val)
{
  m_validInterval = val;
}

void IntervalView::setGripCursor()
{
  this->setCursor(QCursor(Qt::ClosedHandCursor));
}

void IntervalView::setUngripCursor()
{
  this->setCursor(QCursor(Qt::OpenHandCursor));
}

void IntervalView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
  {
    if(event->pos().y() < 4)
      setGripCursor();
    else
      unsetCursor();
    m_presenter.pressed(event->scenePos());
  }
}

void IntervalView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.moved(event->scenePos());
}

void IntervalView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.released(event->scenePos());
  if(event->pos().y() < 4)
    setUngripCursor();
  else
    unsetCursor();
}

bool IntervalView::warning() const
{
  return m_warning;
}

void IntervalView::setWarning(bool warning)
{
  m_warning = warning;
}

const QBrush& IntervalView::intervalColor(const ScenarioStyle& skin) const
{
  // TODO make a switch instead
  if (isSelected())
  {
    return skin.IntervalSelected.getBrush();
  }
  else if (warning())
  {
    return skin.IntervalWarning.getBrush();
  }
  else if (!isValid() || m_state == IntervalExecutionState::Disabled)
  {
    return skin.IntervalInvalid.getBrush();
  }
  else if (m_state == IntervalExecutionState::Muted)
  {
    return skin.IntervalMuted.getBrush();
  }
  else
  {
    return skin.IntervalBase.getBrush();
  }
}

void IntervalView::updateOverlay()
{
  if(m_overlay) m_overlay->update();
  update();
}
void IntervalView::updateLabelPos()
{
  const auto defW = defaultWidth();
  const auto textW = m_labelItem.boundingRect().width();
  const bool vis = m_labelItem.isVisible();
  if(defW > textW && !vis)
  {
    m_labelItem.setVisible(true);
    m_labelItem.setPos(defW / 2. - textW / 2., -17);
  }
  else if (defW <= textW && vis)
  {
    m_labelItem.setVisible(false);
  }
  else if(vis)
  {
    m_labelItem.setPos(defW / 2. - textW / 2., -17);
  }
}

void IntervalView::updateCounterPos()
{
  m_counterItem.setPos(
        defaultWidth() - m_counterItem.boundingRect().width() - 5, 5);
}

void IntervalView::setExecutionState(IntervalExecutionState s)
{
  m_state = s;
  update();
}
}
