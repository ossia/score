#include "NodalIntervalView.hpp"

#include <QGraphicsSceneDragDropEvent>

#include <score/graphics/ZoomItem.hpp>
namespace Scenario
{

NodalIntervalView::NodalIntervalView(NodalIntervalView::ItemsToShow sh, const IntervalModel& model, const Process::Context& ctx, QGraphicsItem* parent)
  : score::EmptyRectItem{parent}
  , m_model{model}
  , m_context{ctx}
  , m_itemsToShow{sh}
  , m_container{new score::EmptyRectItem{this}}
{
  setAcceptDrops(true);
  setAcceptedMouseButtons(Qt::AllButtons);
  // setFlag(ItemHasNoContents, true);
  // setRect(QRectF{0, 0, 1000, 1000});
  const TimeVal r = m_model.duration.defaultDuration();
  for (auto& proc : m_model.processes)
  {
    if(m_itemsToShow == ItemsToShow::OnlyEffects && !(proc.flags() & Process::ProcessFlags::TimeIndependent))
      continue;
    auto item = new Process::NodeItem{proc, m_context, m_container};
    item->setParentDuration(r);
    m_nodeItems.push_back(item);
  }
  m_model.processes.added.connect<&NodalIntervalView::on_processAdded>(*this);
  m_model.processes.removing.connect<&NodalIntervalView::on_processRemoving>(*this);

  con(
        model,
        &IntervalModel::executionFinished,
        this,
        [=] { on_playPercentageChanged(0., TimeVal{}); },
  Qt::QueuedConnection);

  {
    // Zoom handling
    auto item = new score::ZoomItem{this};
    item->setPos(10, 10);

    // TODO proper zooming is done in log space
    connect(item, &score::ZoomItem::zoom, this, [this] {
      auto zoom = m_container->scale();
      if(zoom < 1 && zoom * 1.2 > 1)
        zoom = 1;
      else
        zoom = qBound(0.001, zoom * 1.2, 1000.);
      m_container->setScale(zoom);
    });
    connect(item, &score::ZoomItem::dezoom,
            this, [this] {
      auto zoom = m_container->scale();
      if(zoom < 1 && zoom / 1.2 > 1)
        zoom = 1;
      else
        zoom = qBound(0.001, zoom / 1.2, 1000.);
      m_container->setScale(zoom);
    });

    connect(item, &score::ZoomItem::recenter,
            this, &NodalIntervalView::recenter);
  }
}

void NodalIntervalView::recenter()
{
  auto parentRect = boundingRect();
  auto childRect = enclosingRect();

  double w_ratio = parentRect.width() / childRect.width();
  double h_ratio = parentRect.height() / childRect.height();
  double z = std::min(1., std::min(w_ratio, h_ratio));
  m_container->setScale(z);

  auto childCenter = m_container->mapRectToParent(childRect).center() - m_container->pos();
  auto ourCenter = parentRect.center();
  auto delta = ourCenter - childCenter;

  m_container->setPos(delta);
}

NodalIntervalView::~NodalIntervalView()
{
  qDeleteAll(m_nodeItems);
}

void NodalIntervalView::on_drop(QPointF pos, const QMimeData* data)
{
  m_context.app.interfaces<Scenario::IntervalDropHandlerList>().drop(
        m_context, m_model, pos, *data);
}

void NodalIntervalView::on_playPercentageChanged(double t, TimeVal parent_dur)
{
  t = ossia::clamp(t, 0., 1.);
  for (Process::NodeItem* node : m_nodeItems)
  {
    node->setPlayPercentage(t, parent_dur);
  }
}

void NodalIntervalView::on_processAdded(const Process::ProcessModel& proc)
{
  if(m_itemsToShow == ItemsToShow::OnlyEffects && !(proc.flags() & Process::ProcessFlags::TimeIndependent))
    return;

  // The reason for this loop sucks a bit.
  // The "nodal" small view is created in IntervalModel::on_addProcess when slotAdded is sent,
  // which is called as a response of the nano signal processes.mutable_added.connect<>...
  // But NodalIntervalView adds itself to the callback list: this means that
  // after creation which already creates a node for the process, we get the item duplicated here.
  for (auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it)
  {
    if (&(*it)->model() == &proc)
    {
      return;
    }
  }

  auto item = new Process::NodeItem{proc, m_context, m_container};
  item->setParentDuration(m_model.duration.defaultDuration());
  m_nodeItems.push_back(item);
}

void NodalIntervalView::on_processRemoving(const Process::ProcessModel& model)
{
  for (auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it)
  {
    if (&(*it)->model() == &model)
    {
      delete (*it);
      m_nodeItems.erase(it);
      return;
    }
  }
}

void NodalIntervalView::on_zoomRatioChanged(ZoomRatio ratio)
{
  // TODO should be "on model duration changed"
  const TimeVal r = m_model.duration.defaultDuration();
  for (Process::NodeItem* node : m_nodeItems)
  {
    node->setParentDuration(r);
  }
}

QRectF NodalIntervalView::enclosingRect() const noexcept
{
  if (m_nodeItems.empty())
    return {};
  double x0{std::numeric_limits<double>::max()}, y0{x0},
  x1{std::numeric_limits<double>::lowest()}, y1{x1};

  for (QGraphicsItem* item : m_nodeItems)
  {
    const auto pos = item->pos();
    const auto r = item->boundingRect();
    if (x0 > pos.x())
      x0 = pos.x();
    if (y0 > pos.y())
      y0 = pos.y();
    if (x1 < pos.x() + r.width())
      x1 = pos.x() + r.width();
    if (y1 < pos.y() + r.height())
      y1 = pos.y() + r.height();
  }

  x0 -= (0.1 * (x1 - x0));
  y0 -= (0.1 * (x1 - x0));
  const double w = 1.1 * (x1 - x0);
  const double h = 1.1 * (y1 - y0);

  return {x0, y0, w, h};
}

void NodalIntervalView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void NodalIntervalView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void NodalIntervalView::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void NodalIntervalView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  on_drop(event->pos(), event->mimeData());
  event->accept();
}

void NodalIntervalView::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
  m_pressedPos = e->scenePos();
  e->accept();
}

void NodalIntervalView::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
  const auto delta = e->scenePos() - m_pressedPos;
  m_container->setPos(m_container->pos() + delta);
  m_pressedPos = e->scenePos();
  e->accept();
}

void NodalIntervalView::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
  mouseMoveEvent(e);
}
}


