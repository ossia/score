// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/IntervalMenuOverlay.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>

#include <Process/Dataflow/CableItem.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/widgets/MimeData.hpp>
#include <QCursor>
#include <QDrag>
#include <QMimeData>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::IntervalView)
namespace Scenario
{
IntervalView::IntervalView(IntervalPresenter& presenter, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_leftBrace{*this, this}
    , m_rightBrace{*this, this}
    , m_counterItem{score::Skin::instance().Light.main, this}
    , m_presenter{presenter}
    , m_selected{false}
    , m_infinite{false}
    , m_validInterval{true}
    , m_warning{false}
    , m_waiting{false}
    , m_dropTarget{false}
    , m_state{}
{
  setAcceptHoverEvents(true);
  setAcceptDrops(true);

  m_leftBrace.setX(minWidth());
  m_leftBrace.hide();

  m_rightBrace.setX(maxWidth());
  m_rightBrace.hide();

  const auto& skin = score::Skin::instance();

  m_counterItem.setFont(skin.Medium7Pt);
  m_counterItem.setAcceptedMouseButtons(Qt::MouseButton::NoButton);
  m_counterItem.setAcceptHoverEvents(false);
}

IntervalView::~IntervalView()
{
  for (auto item : childItems())
  {
    if (item->type() == Dataflow::CableItem::Type)
    {
      item->setParentItem(nullptr);
    }
  }
  //delete m_overlay;
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
  if(m_waiting && !e)
  {
    m_execPing.start();
  }

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
      || (width > 0 && (playedSolidPath.isEmpty())))
  {
    m_playWidth = width;
    updatePlayPaths();
    // Already called in DisplayedElementsPresenter::on_intervalExecutionTimer() and
    // ScenarioPresenter::on_intervalExecutionTimer()
    // update();
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

const score::Brush& IntervalView::intervalColor(const Process::Style& skin) const
{
  if(Q_UNLIKELY(m_dropTarget))
  {
    return skin.IntervalDropTarget();
  }
  else if (Q_UNLIKELY(m_selected))
  {
    return skin.IntervalSelected();
  }
  else if (Q_UNLIKELY(m_warning))
  {
    return skin.IntervalWarning();
  }
  else if (Q_UNLIKELY(!m_validInterval || m_state == IntervalExecutionState::Disabled))
  {
    return skin.IntervalInvalid();
  }
  else if (Q_UNLIKELY(m_state == IntervalExecutionState::Muted))
  {
    return skin.IntervalMuted();
  }
  else
  {
    return skin.IntervalBase();
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
  setDropTarget(true);
  event->accept();
}

void IntervalView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragLeaveEvent(event);
  setDropTarget(m_presenter.header()->contains(mapToItem(m_presenter.header(), event->pos())));
  event->accept();
}

void IntervalView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
  setDropTarget(false);
  update();

  event->accept();
}
}

QPainterPath Scenario::IntervalView::shape() const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  QPainterPath p;
  p.addRect({x, -1., rectW, intervalAndRackHeight()});
  return p;
}

QPainterPath Scenario::IntervalView::opaqueArea() const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  QPainterPath p;
  p.addRect({x, -1., rectW, intervalAndRackHeight()});
  return p;
}

bool Scenario::IntervalView::contains(const QPointF& pt) const
{
  qreal x = std::min(0., minWidth());
  qreal rectW = infinite() ? defaultWidth() : maxWidth();
  rectW -= x;
  return QRectF{x, -1., rectW, intervalAndRackHeight()}.contains(pt);
}
