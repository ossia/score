#include "NodalIntervalView.hpp"

#include <Scenario/Application/Drops/DropOnCable.hpp>
#include <Scenario/Application/Drops/DropProcessInInterval.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/graphics/ZoomItem.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsView>
#include <QTimer>

#include <wobjectimpl.h>

namespace Scenario
{
struct NodalContainer : public score::EmptyRectItem
{
  W_OBJECT(NodalContainer)
public:
  using score::EmptyRectItem::EmptyRectItem;

  int type() const noexcept { return QGraphicsItem::UserType + 5555; }
};
}
W_OBJECT_IMPL(Scenario::NodalIntervalView)
W_OBJECT_IMPL(Scenario::NodalContainer)
namespace Scenario
{
constexpr double g_zoom_base = 1.2;
NodalIntervalView::NodalIntervalView(
    NodalIntervalView::ItemsToShow sh, const IntervalModel& model,
    const Process::Context& ctx, QGraphicsItem* parent)
    : score::EmptyRectItem{parent}
    , m_model{model}
    , m_context{ctx}
    , m_itemsToShow{sh}
    , m_container{new NodalContainer{this}}
{
  setAcceptDrops(true);
  setAcceptedMouseButtons(Qt::AllButtons);
  // setFlag(ItemHasNoContents, true);
  // setRect(QRectF{0, 0, 1000, 1000});
  const TimeVal r = m_model.duration.defaultDuration();
  for(auto& proc : m_model.processes)
  {
    if(m_itemsToShow == ItemsToShow::OnlyEffects
       && !(proc.flags() & Process::ProcessFlags::TimeIndependent))
      continue;
    auto item = new Process::NodeItem{proc, m_context, r, m_container};
    m_nodeItems.push_back(item);
    connect(
        item, &Process::NodeItem::dropReceived, this, &NodalIntervalView::on_dropOnNode);
  }
  m_model.processes.added.connect<&NodalIntervalView::on_processAdded>(*this);
  m_model.processes.removing.connect<&NodalIntervalView::on_processRemoving>(*this);

  con(
      model, &IntervalModel::executionEvent, this,
      [this](IntervalExecutionEvent ev) {
    if(ev == IntervalExecutionEvent::Finished)
      on_playPercentageChanged(0., TimeVal{});
      },
      Qt::QueuedConnection);

  {
    // Zoom handling
    auto item = new score::ZoomItem{this};
    item->setPos(10, 10);

    connect(item, &score::ZoomItem::zoom, this, &NodalIntervalView::zoomPlus);
    connect(item, &score::ZoomItem::dezoom, this, &NodalIntervalView::zoomMinus);

    connect(item, &score::ZoomItem::recenter, this, &NodalIntervalView::recenter);
    connect(item, &score::ZoomItem::rescale, this, &NodalIntervalView::rescale);
    connect(
        this, &score::EmptyRectItem::sizeChanged, this,
        &NodalIntervalView::recenterRelativeToView);

    if(parent)
    {
      if(auto v = getView(*parent))
      {
        auto gv = static_cast<Scenario::ProcessGraphicsView*>(v);
        connect(gv, &ProcessGraphicsView::visibleRectChanged, this, [this] {
          recenterRelativeToView();
        }, Qt::DirectConnection);
      }
    }
  }
  QTimer::singleShot(1, this, &NodalIntervalView::recenterRelativeToView);
}

void NodalIntervalView::zoomPlus()
{
  auto newLevel = m_zoomLevel + 1.0;
  if(newLevel <= 0.5 && newLevel >= -0.5)
    newLevel = 0.;
  zoomTo(newLevel);
}

void NodalIntervalView::zoomMinus()
{
  auto newLevel = m_zoomLevel - 1.0;
  if(newLevel <= 0.5 && newLevel >= -0.5)
    newLevel = 0.;
  zoomTo(newLevel);
}

void NodalIntervalView::recenterRelativeToView()
{
  auto v = getView(*this);
  if(!v)
    return;
  auto parentRect = boundingRect();
  auto childRect = enclosingRect();

  auto viewTopLeft = mapFromScene(v->mapToScene(0, 0));
  auto viewBottomRight = mapFromScene(v->mapToScene(v->width(), v->height()));
  auto viewRect = QRectF{viewTopLeft, viewBottomRight};
  auto visibleRect = viewRect.intersected(parentRect);

  auto childCenter
      = m_container->mapRectToParent(childRect).center() - m_container->pos();
  auto ourCenter = visibleRect.center();
  auto delta = ourCenter - childCenter;

  m_container->setPos(delta + m_model.nodalOffset());
}

void NodalIntervalView::recenter()
{
  auto parentRect = boundingRect();
  auto childRect = enclosingRect();

  double w_ratio = parentRect.width() / childRect.width();
  double h_ratio = parentRect.height() / childRect.height();
  double z = std::clamp(std::min(w_ratio, h_ratio), 0.01, 1.0);
  m_container->setScale(z);

  auto childCenter
      = m_container->mapRectToParent(childRect).center() - m_container->pos();
  auto ourCenter = parentRect.center();
  auto delta = ourCenter - childCenter;

  m_container->setPos(delta);
  const_cast<IntervalModel&>(m_model).setNodalOffset(QPointF{});
}

void NodalIntervalView::rescale()
{
  recenterRelativeToView();
  auto parentRect = boundingRect();
  auto childRect = enclosingRect();

  m_container->setScale(1.0);

  auto childCenter
      = m_container->mapRectToParent(childRect).center() - m_container->pos();
  auto ourCenter = parentRect.center();
  auto delta = ourCenter - childCenter;

  m_container->setPos(delta + m_model.nodalOffset());
}

NodalIntervalView::~NodalIntervalView()
{
  qDeleteAll(m_nodeItems);
}

void NodalIntervalView::on_drop(QPointF pos, const QMimeData* data)
{
  const bool ok = m_context.app.interfaces<Scenario::IntervalDropHandlerList>().drop(
      m_context, m_model, m_container->mapFromParent(pos), *data);
  if(ok)
    return;

  Scenario::DropProcessInInterval d;
  d.drop(m_context, m_model, pos, *data);
}

void NodalIntervalView::on_playPercentageChanged(double t, TimeVal parent_dur)
{
  t = ossia::max(t, 0.);
  for(Process::NodeItem* node : m_nodeItems)
  {
    node->setPlayPercentage(t, parent_dur);
  }
}

void NodalIntervalView::on_processAdded(const Process::ProcessModel& proc)
{
  if(m_itemsToShow == ItemsToShow::OnlyEffects
     && !(proc.flags() & Process::ProcessFlags::TimeIndependent))
    return;

  // The reason for this loop sucks a bit.
  // The "nodal" small view is created in IntervalModel::on_addProcess when slotAdded is sent,
  // which is called as a response of the nano signal processes.mutable_added.connect<>...
  // But NodalIntervalView adds itself to the callback list: this means that
  // after creation which already creates a node for the process, we get the item duplicated here.
  for(auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it)
  {
    if(&(*it)->model() == &proc)
    {
      return;
    }
  }

  auto item = new Process::NodeItem{
      proc, m_context, m_model.duration.defaultDuration(), m_container};
  connect(
      item, &Process::NodeItem::dropReceived, this, &NodalIntervalView::on_dropOnNode);
  m_nodeItems.push_back(item);
}

void NodalIntervalView::on_processRemoving(const Process::ProcessModel& model)
{
  for(auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it)
  {
    if(&(*it)->model() == &model)
    {
      delete(*it);
      m_nodeItems.erase(it);
      return;
    }
  }
}

void NodalIntervalView::on_zoomRatioChanged(ZoomRatio ratio)
{
  // TODO should be "on model duration changed"
  const TimeVal r = m_model.duration.defaultDuration();
  for(Process::NodeItem* node : m_nodeItems)
  {
    node->setParentDuration(r);
  }
}

QRectF NodalIntervalView::enclosingRect() const noexcept
{
  if(m_nodeItems.empty())
    return QRectF{-100., -100., 200., 200.};
  double x0{std::numeric_limits<double>::max()}, y0{x0},
      x1{std::numeric_limits<double>::lowest()}, y1{x1};

  for(QGraphicsItem* item : m_nodeItems)
  {
    const auto pos = item->pos();
    const auto r = item->boundingRect();
    if(x0 > pos.x())
      x0 = pos.x();
    if(y0 > pos.y())
      y0 = pos.y();
    if(x1 < pos.x() + r.width())
      x1 = pos.x() + r.width();
    if(y1 < pos.y() + r.height())
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
  this->m_context.selectionStack.deselect();
  auto focus = Process::ProcessFocusManager::get(this->m_context);
  focus->focusNothing();

  m_pressedPos = e->scenePos();
  e->accept();

  score::SelectionDispatcher disp{m_context.selectionStack};
  disp.select(m_model);
}

void NodalIntervalView::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
  const auto delta = e->scenePos() - m_pressedPos;
  m_container->setPos(m_container->pos() + delta);
  m_pressedPos = e->scenePos();
  e->accept();
  const_cast<IntervalModel&>(m_model).setNodalOffset(m_model.nodalOffset() + delta);
}

void NodalIntervalView::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
  const auto delta = e->scenePos() - m_pressedPos;

  m_container->setPos(m_container->pos() + delta);
  m_pressedPos = e->scenePos();
  e->accept();

  const_cast<IntervalModel&>(m_model).setNodalOffset(m_model.nodalOffset() + delta);
}

void NodalIntervalView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void NodalIntervalView::wheelEvent(QGraphicsSceneWheelEvent* event)
{
  static constexpr double sensitivity = 120.0;
  double numDegrees = event->delta();
  if(numDegrees == 0)
  {
    event->ignore();
    return;
  }

  QPointF anchor = event->pos();
  QPointF localAnchor = m_container->mapFromParent(anchor);

  double scrollStep = numDegrees / sensitivity;
  m_zoomLevel = std::clamp(m_zoomLevel + scrollStep, -10.0, 5.0);
  double newScale = std::pow(g_zoom_base, m_zoomLevel);
  m_container->setScale(newScale);

  QPointF newAnchorPos = m_container->mapToParent(localAnchor);
  m_container->setPos(m_container->pos() + (anchor - newAnchorPos));

  event->accept();
}

void NodalIntervalView::zoomTo(double newZoomLevel)
{
  newZoomLevel = std::clamp(newZoomLevel, -10.0, 5.0);
  if(newZoomLevel == m_zoomLevel)
    return;

  QPointF anchor;
  if(auto v = getView(*this))
  {
    const auto viewTopLeft = mapFromScene(v->mapToScene(0, 0));
    const auto viewBottomRight = mapFromScene(v->mapToScene(v->width(), v->height()));
    const auto visibleRect
        = QRectF{viewTopLeft, viewBottomRight}.intersected(boundingRect());
    anchor = visibleRect.center();
  }
  else
  {
    anchor = boundingRect().center();
  }

  const QPointF localAnchor = m_container->mapFromParent(anchor);

  m_zoomLevel = newZoomLevel;
  double newScale = std::pow(g_zoom_base, m_zoomLevel);
  m_container->setScale(newScale);

  const QPointF newAnchorPos = m_container->mapToParent(localAnchor);
  m_container->setPos(m_container->pos() + (anchor - newAnchorPos));
}

void NodalIntervalView::on_dropOnNode(const QPointF& pos, const QMimeData& mime)
{
  auto item = static_cast<Process::NodeItem*>(QObject::sender());
  if(!item)
    return;

  auto& doc = score::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(
      m_context.document);
  auto drop = new Scenario::DropOnNode{*item, doc, m_context};
  drop->drop(mime);
  drop->deleteLater();
}
}
