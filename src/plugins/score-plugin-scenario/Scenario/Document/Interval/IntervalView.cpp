// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalView.hpp"

#include "IntervalMenuOverlay.hpp"
#include "IntervalModel.hpp"
#include "IntervalPresenter.hpp"

#include <Process/Dataflow/CableItem.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <QCursor>
#include <QDrag>
#include <QGraphicsSceneEvent>
#include <QMimeData>
#include <QtGlobal>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::IntervalView)
namespace Scenario
{
IntervalView::IntervalView(IntervalPresenter& presenter, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_leftBrace{*this, this}
    , m_rightBrace{*this, this}
    , m_labelItem{Process::Style::instance().ConditionWaiting, this}
    , m_counterItem{score::ColorRef(&score::Skin::Light), this}
    , m_presenter{presenter}
    , m_selected{false}
    , m_infinite{false}
    , m_validInterval{true}
    , m_warning{false}
    , m_hasFocus{false}
    , m_waiting{false}
    , m_dropTarget{false}
{
  setAcceptHoverEvents(true);
  setAcceptDrops(true);

  m_leftBrace.setX(minWidth());
  m_leftBrace.hide();

  m_rightBrace.setX(maxWidth());
  m_rightBrace.hide();

  const auto& skin = score::Skin::instance();
  m_labelItem.setFont(skin.Medium12Pt);
  m_labelItem.setPos(0, -16);
  m_labelItem.setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_labelItem.setAcceptHoverEvents(false);

  m_counterItem.setFont(skin.Medium7Pt);
  m_counterItem.setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_counterItem.setAcceptHoverEvents(false);
}

IntervalView::~IntervalView()
{
  for (auto item : childItems())
  {
    if (item->type() == Dataflow::CableItem::static_type())
    {
      item->setParentItem(nullptr);
    }
  }
  delete m_overlay;
}

void IntervalView::setInfinite(bool infinite)
{
  if (m_infinite != infinite)
  {
    prepareGeometryChange();

    m_infinite = infinite;
    updatePaths();
    updatePlayPaths();
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
  if (m_defaultWidth != width)
  {
    prepareGeometryChange();
    m_defaultWidth = width;
    updatePaths();
    updatePlayPaths();
    update();
  }
}

void IntervalView::setMaxWidth(bool infinite, double max)
{
  if (infinite != m_infinite || max != m_maxWidth)
  {
    prepareGeometryChange();

    setInfinite(infinite);
    if (!infinite)
    {
      m_maxWidth = max;
    }
    updatePaths();
    updatePlayPaths();
    update();
  }
}

void IntervalView::setMinWidth(double min)
{
  if (min != m_minWidth)
  {
    prepareGeometryChange();
    m_minWidth = min;
    updatePaths();
    updatePlayPaths();
    update();
  }
}

void IntervalView::setHeight(double height)
{
  if (m_height != height)
  {
    prepareGeometryChange();
    m_height = height;
    updatePaths();
    updatePlayPaths();
    update();
  }
}

double IntervalView::setPlayWidth(double width)
{
  const auto v = std::abs(m_playWidth - width);
  if (v > 1.
      || (width > 0
          && (playedSolidPath.isEmpty() || playedDashedPath.isEmpty())))
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
  if (event->pos().y() < 4)
    setGripCursor();
  else
    unsetCursor();
  m_presenter.pressed(event->scenePos());
}

void IntervalView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->buttons() & Qt::MiddleButton)
  {
    if (auto si = dynamic_cast<Scenario::ScenarioInterface*>(
            presenter().model().parent()))
    {
      auto obj = copySelectedElementsToJson(
          *const_cast<ScenarioInterface*>(si), m_presenter.context());

      if (!obj.empty())
      {
        QDrag d{this};
        auto m = new QMimeData;
        QJsonDocument doc{obj};
        ;
        m->setData(
            score::mime::scenariodata(), doc.toJson(QJsonDocument::Indented));
        d.setMimeData(m);
        d.exec();
      }
    }
  }
  m_presenter.moved(event->scenePos());
}

void IntervalView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.released(event->scenePos());
  if (event->pos().y() < 4)
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

const QBrush& IntervalView::intervalColor(const Process::Style& skin) const
{
  if(m_dropTarget)
  {
    return skin.IntervalDropTarget.getBrush();
  }
  else if (m_selected)
  {
    return skin.IntervalSelected.getBrush();
  }
  else if (m_warning)
  {
    return skin.IntervalWarning.getBrush();
  }
  else if (!m_validInterval || m_state == IntervalExecutionState::Disabled)
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
  if (m_overlay)
    m_overlay->update();
  update();
}
void IntervalView::updateLabelPos()
{
  const auto defW = defaultWidth();
  const auto textW = m_labelItem.boundingRect().width();
  const bool vis = m_labelItem.isVisible();
  if (defW > textW && !vis)
  {
    m_labelItem.setVisible(true);
    m_labelItem.setPos(defW / 2. - textW / 2., -17);
  }
  else if (defW <= textW && vis)
  {
    m_labelItem.setVisible(false);
  }
  else if (vis)
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

void IntervalView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragEnterEvent(event);
  m_dropTarget = true;
  updateOverlay();
  event->accept();
}

void IntervalView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragLeaveEvent(event);
  m_dropTarget = false;
  updateOverlay();
  event->accept();
}

void IntervalView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
  m_dropTarget = false;

  event->accept();
}
}

QPainterPath Scenario::IntervalView::shape() const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  QPainterPath p;
  p.addRect({x, -12., rectW, 24.});
  return p;
}

QPainterPath Scenario::IntervalView::opaqueArea() const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  QPainterPath p;
  p.addRect({x, -12., rectW, 24.});
  return p;
}

bool Scenario::IntervalView::contains(const QPointF& pt) const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  return QRectF{x, -12., rectW, 24.}.contains(pt);
}
