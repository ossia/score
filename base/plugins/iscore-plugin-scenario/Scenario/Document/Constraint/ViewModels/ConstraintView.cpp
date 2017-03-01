#include <QGraphicsSceneEvent>
#include <QtGlobal>

#include "ConstraintPresenter.hpp"
#include "ConstraintView.hpp"
#include "ConstraintMenuOverlay.hpp"
#include <Scenario/Document/Constraint/ViewModels/Temporal/Braces/LeftBrace.hpp>

namespace Scenario
{
ConstraintView::ConstraintView(
    ConstraintPresenter& presenter, QQuickPaintedItem* parent)
    : GraphicsItem{parent}
    , m_labelItem{new SimpleTextItem{this}}
    , m_counterItem{new SimpleTextItem{this}}
    , m_presenter{presenter}
{
  setAcceptedMouseButtons(Qt::AllButtons);
  setAcceptHoverEvents(true);
  m_leftBrace = new LeftBraceView{*this, this};
  m_leftBrace->setX(minWidth());
  m_leftBrace->setVisible(false);

  m_rightBrace = new RightBraceView{*this, this};
  m_rightBrace->setX(maxWidth());
  m_rightBrace->setVisible(false);

  const int fontSize = 12;
  auto f = iscore::Skin::instance().SansFont;
  f.setBold(false);
  f.setPointSize(fontSize);
  f.setStyleStrategy(QFont::NoAntialias);
  m_labelItem->setFont(f);
  m_labelItem->setPosition(QPointF(0, -16));
  m_labelItem->setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_labelItem->setAcceptHoverEvents(false);

  f.setPointSize(7);
  f.setStyleStrategy(QFont::NoAntialias);
  f.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  m_counterItem->setFont(f);
  m_counterItem->setColor(iscore::ColorRef(&iscore::Skin::Light));
  m_counterItem->setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_counterItem->setAcceptHoverEvents(false);
}

ConstraintView::~ConstraintView()
{
  delete m_overlay;
}

void ConstraintView::setInfinite(bool infinite)
{
  if(m_infinite != infinite)
  {
    m_infinite = infinite;
    updatePaths();
  }
}

void ConstraintView::setExecuting(bool e)
{
  m_executing = e;
  update();
}

void ConstraintView::setDefaultWidth(double width)
{
  if(m_defaultWidth != width)
  {
    m_defaultWidth = width;
    updateSize();
    updatePaths();
  }
}

void ConstraintView::setMaxWidth(bool infinite, double max)
{
  if(infinite != m_infinite || max != m_maxWidth)
  {
    setInfinite(infinite);
    if (!infinite)
    {
      m_maxWidth = max;
    }
    updateSize();
    updatePaths();
  }
}

void ConstraintView::setMinWidth(double min)
{
  if(min != m_minWidth)
  {
    m_minWidth = min;
    updatePaths();
  }
}

void ConstraintView::setHeight(double h)
{
  if(height() != h)
  {
    QQuickPaintedItem::setHeight(h);
    updateSize();
    updatePaths();
  }
}

void ConstraintView::setPlayWidth(double width)
{
  if(width != m_playWidth)
  {
    m_playWidth = width;
    updatePaths();
  }
}

void ConstraintView::setValid(bool val)
{
  m_validConstraint = val;
}

void ConstraintView::setSelected(bool selected)
{
  m_selected = selected;
  setZ(m_selected ? ZPos::SelectedConstraint : ZPos::Constraint);
  enableOverlay(selected);
  update();
}

void ConstraintView::enableOverlay(bool selected)
{

}

void ConstraintView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    emit m_presenter.pressed(mapToScene(event->localPos()));
}

void ConstraintView::mouseMoveEvent(QMouseEvent* event)
{
  emit m_presenter.moved(mapToScene(event->localPos()));
}

void ConstraintView::mouseReleaseEvent(QMouseEvent* event)
{
  emit m_presenter.released(mapToScene(event->localPos()));
}

bool ConstraintView::warning() const
{
  return m_warning;
}

void ConstraintView::setWarning(bool warning)
{
  m_warning = warning;
}

QBrush ConstraintView::constraintColor(const ScenarioStyle& skin) const
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

bool ConstraintView::shadow() const
{
  return m_shadow;
}

void ConstraintView::setShadow(bool shadow)
{
  m_shadow = shadow;
  if(m_overlay) m_overlay->update();
  update();
}
void ConstraintView::updateLabelPos()
{
  m_labelItem->setPosition(QPointF(
        defaultWidth() / 2. - m_labelItem->boundingRect().width() / 2., -17));
}

void ConstraintView::updateCounterPos()
{
  m_counterItem->setPosition(QPointF(
        defaultWidth() - m_counterItem->boundingRect().width() - 5, 5));
}

void ConstraintView::updateOverlayPos()
{
  if(m_overlay)
    m_overlay->setPosition(QPointF(defaultWidth() / 2. - m_overlay->boundingRect().width() / 2, -10));
}

void ConstraintView::setExecutionState(ConstraintExecutionState s)
{
  m_state = s;
  update();
}
}
